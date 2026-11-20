#!/usr/bin/env python3
#
# A stand-in FujiNet for the host simulator's tests: plays FujiNet-PC's
# side of "SmartPort over SLIP" (see src/port/fujinet.c), connecting to
# the simulator on 127.0.0.1:PORT.
#
#   python3 fake_fujinet.py images DIR
#   python3 fake_fujinet.py serve PORT DIR [vanish]
#
# "images" writes into DIR the disk images the tests use:
#
#   fuji.po   FujiNet's first disk: a boot block that makes SmartPort
#             calls ($C70D) and sends what they got back to FujiNet;
#   local.po  a disk for KEGS's own s7d1: a boot block that fills the
#             text screen with blocks.
#
# "serve" offers three units: 1, a disk holding fuji.po; 2, a disk whose
# block N is filled with (N * 7 + i) & $FF; 3, "THE_FUJI" (no blocks). It
# saves every block written as DIR/written_U_B.bin, logs every request
# to DIR/requests.txt, and stops when the simulator closes the
# connection. With "vanish" it closes the connection at the first block
# read, as a FujiNet-PC that was stopped.
#

import os
import socket
import sys
import time

END, ESC, ESC_END, ESC_ESC = 0xC0, 0xDB, 0xDC, 0xDD

# The boot block for FujiNet's disk, at $800 (the Apple starts it at $801).
#   read block 5 of unit 2 to $1000, write it back as block 6;
#   status of unit 0 (the unit count) to $1200, X (its length) to $1208;
#   read block 0 of unit 4 (KEGS's s7d1, after FujiNet's 3) to $1400, the
#   error (A) to $1209;
#   write $1200 as block 7 of unit 2, $1400 as block 8;
#   control code $AA to unit 3 with the list 03 00 "ABC".
FUJI_BOOT = bytes([
    0x01,
    0x20, 0x0D, 0xC7, 0x01, 0x34, 0x08,   # $801 JSR $C70D: READ, L1
    0x20, 0x0D, 0xC7, 0x02, 0x3B, 0x08,   # $807 WRITE, L2
    0x20, 0x0D, 0xC7, 0x00, 0x42, 0x08,   # $80D STATUS, L3
    0x8E, 0x08, 0x12,                     # $813 STX $1208
    0x20, 0x0D, 0xC7, 0x01, 0x47, 0x08,   # $816 READ, L4
    0x8D, 0x09, 0x12,                     # $81C STA $1209
    0x20, 0x0D, 0xC7, 0x02, 0x4E, 0x08,   # $81F WRITE, L5
    0x20, 0x0D, 0xC7, 0x02, 0x55, 0x08,   # $825 WRITE, L6
    0x20, 0x0D, 0xC7, 0x04, 0x5C, 0x08,   # $82B CONTROL, L7
    0x4C, 0x31, 0x08,                     # $831 JMP $831
    0x03, 0x02, 0x00, 0x10, 0x05, 0x00, 0x00,   # $834 L1
    0x03, 0x02, 0x00, 0x10, 0x06, 0x00, 0x00,   # $83B L2
    0x03, 0x00, 0x00, 0x12, 0x00,               # $842 L3
    0x03, 0x04, 0x00, 0x14, 0x00, 0x00, 0x00,   # $847 L4
    0x03, 0x02, 0x00, 0x12, 0x07, 0x00, 0x00,   # $84E L5
    0x03, 0x02, 0x00, 0x14, 0x08, 0x00, 0x00,   # $855 L6
    0x03, 0x03, 0x61, 0x08, 0xAA,               # $85C L7
    0x03, 0x00, 0x41, 0x42, 0x43,               # $861 the control list
])

# The boot block for KEGS's own disk: inverse spaces all over the screen.
LOCAL_BOOT = bytes([
    0x01,
    0xA9, 0x20,              # $801 LDA #$20
    0xA2, 0x00,              # $803 LDX #0
    0x9D, 0x00, 0x04,        # $805 STA $400,X
    0x9D, 0x00, 0x05,        # $808 STA $500,X
    0x9D, 0x00, 0x06,        # $80B STA $600,X
    0x9D, 0x00, 0x07,        # $80E STA $700,X
    0xE8,                    # $811 INX
    0xD0, 0xF1,              # $812 BNE $805
    0x4C, 0x14, 0x08,        # $814 JMP $814
])

BLOCKS = 280


def image(boot):
    data = bytearray(BLOCKS * 512)
    data[:len(boot)] = boot
    # Something recognisable after the boot code.
    for i in range(len(boot), 512):
        data[i] = (i * 3 + 1) & 0xff
    return data


def pattern(block):
    return bytes(((block * 7 + i) & 0xff) for i in range(512))


def slip(data):
    out = bytearray([END])
    for b in data:
        if b == END:
            out += bytes([ESC, ESC_END])
        elif b == ESC:
            out += bytes([ESC, ESC_ESC])
        else:
            out.append(b)
    out.append(END)
    return bytes(out)


def dib(status, blocks, name, kind, subtype):
    name = name.encode()
    return (bytes([status, blocks & 0xff, (blocks >> 8) & 0xff, blocks >> 16,
                   len(name)]) + name.ljust(16, b' ') + bytes([kind, subtype, 0x01, 0x0f]))


def serve(port, folder, vanish):
    disk1 = image(FUJI_BOOT)
    log = open(os.path.join(folder, 'requests.txt'), 'w')
    s = None
    for _ in range(600):
        try:
            s = socket.create_connection(('127.0.0.1', port))
            break
        except OSError:
            time.sleep(0.1)
    if s is None:
        log.write('never connected\n')
        return
    buf = bytearray()
    while True:
        while True:
            start = buf.find(bytes([END]))
            if start >= 0:
                end = buf.find(bytes([END]), start + 1)
                if end == start + 1:
                    del buf[:start + 1]
                    continue
                if end > start:
                    break
            got = s.recv(65536)
            if not got:
                log.close()
                return
            buf += got
        raw = bytes(buf[start + 1:end])
        del buf[:end + 1]
        p = bytearray()
        i = 0
        while i < len(raw):
            if raw[i] == ESC:
                i += 1
                p.append(END if raw[i] == ESC_END else ESC)
            else:
                p.append(raw[i])
            i += 1
        seq, cmd, pcount, unit = p[0], p[1], p[2], p[3]
        params = bytes(p[6:11])
        data = bytes(p[11:])
        log.write('cmd %d pcount %d unit %d params %s data %s\n'
                  % (cmd, pcount, unit, params.hex(), data[:16].hex()))
        log.flush()
        status, reply = 0, b''
        if cmd == 5:                      # INIT: unit 3 is the last
            status = 0xff if unit >= 3 else 0
        elif cmd == 0:                    # STATUS
            code = params[0]
            if unit == 1 and code == 0:
                reply = bytes([0xf8, BLOCKS & 0xff, BLOCKS >> 8, 0])
            elif unit == 2 and code == 0:
                reply = bytes([0xf8, BLOCKS & 0xff, BLOCKS >> 8, 0])
            elif code == 3:
                reply = {1: dib(0xf8, BLOCKS, 'FUJINET_DISK_0', 2, 0),
                         2: dib(0xf8, BLOCKS, 'FUJINET_DISK_1', 2, 0),
                         3: dib(0x30, 0, 'THE_FUJI', 0x10, 0)}.get(unit, b'')
            else:
                reply = bytes([0x30, 0, 0, 0])
        elif cmd == 1:                    # READ BLOCK
            if vanish:
                s.close()
                log.write('vanished\n')
                log.close()
                return
            block = params[0] | params[1] << 8 | params[2] << 16
            if unit == 1:
                reply = bytes(disk1[block * 512:(block + 1) * 512])
            elif unit == 2:
                reply = pattern(block)
            else:
                status = 0x21
        elif cmd == 2:                    # WRITE BLOCK
            block = params[0] | params[1] << 8 | params[2] << 16
            with open(os.path.join(folder, 'written_%d_%d.bin' % (unit, block)), 'wb') as f:
                f.write(data)
        elif cmd == 4:                    # CONTROL
            pass
        else:
            status = 0x01
        s.sendall(slip(bytes([seq, status]) + reply))


if __name__ == '__main__':
    if sys.argv[1] == 'images':
        folder = sys.argv[2]
        open(os.path.join(folder, 'fuji.po'), 'wb').write(image(FUJI_BOOT))
        open(os.path.join(folder, 'local.po'), 'wb').write(image(LOCAL_BOOT))
        sys.stdout.write(pattern(5).hex() + '\n')
    else:
        serve(int(sys.argv[2]), sys.argv[3], len(sys.argv) > 4 and sys.argv[4] == 'vanish')
