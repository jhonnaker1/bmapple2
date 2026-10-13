#!/usr/bin/env python3
#
# Makes a stand-in Apple II ROM for the host simulator's tests.
#
# The real Apple ROMs are copyrighted, so the tests can't use them. This
# writes a 32K "ROM" into the directory given: the size of an Apple IIe ROM
# file, so KEGS runs it as a IIe, with the last 16K at $C000-$FFFF (KEGS's
# IIe ROM layout; see its doc/README.ROM.files.txt). It holds a small 6502
# program that, on the 40 column text screen:
#
#   - counts frames (in hex) at the top left, on row 0;
#   - shows on row 6 a block while any key is down ($C010), and one for each
#     of buttons 0 and 1 ($C061, $C062);
#   - shows on row 7 paddles 0 and 1 (in hex, read as the ROM's PREAD does);
#   - clicks the speaker ($C030) every frame, a short buzz.
#
# Frames are counted at the start of each vertical blank ($C019), so a
# screenshot taken a frame later always differs at the top.
#
#   python3 make_test_rom.py DIR
#

import sys

SPACE = 0xa0          # normal space
BLOCK = 0x20          # inverse space: a solid block
ROW0, ROW6, ROW7 = 0x400, 0x700, 0x780


class Asm:
    """Just enough of a 6502 assembler: the opcodes below, with labels."""

    def __init__(self, origin):
        self.origin = origin
        self.code = bytearray()
        self.labels = {}
        self.fixups = []  # (offset, label, 'abs' or 'rel')

    def here(self):
        return self.origin + len(self.code)

    def label(self, name):
        self.labels[name] = self.here()

    def byte(self, *values):
        self.code.extend(values)

    def word(self, value):
        if isinstance(value, str):
            self.fixups.append((len(self.code), value, 'abs'))
            value = 0
        self.code.extend((value & 0xff, value >> 8))

    def op(self, opcode, operand=None, size=0):
        self.code.append(opcode)
        if size == 1:
            self.code.append(operand)
        elif size == 2:
            self.word(operand)

    def branch(self, opcode, target):
        self.code.append(opcode)
        self.fixups.append((len(self.code), target, 'rel'))
        self.code.append(0)

    def finish(self):
        for offset, name, kind in self.fixups:
            target = self.labels[name]
            if kind == 'abs':
                self.code[offset:offset + 2] = bytes((target & 0xff, target >> 8))
            else:
                delta = target - (self.origin + offset + 1)
                assert -128 <= delta <= 127, name
                self.code[offset] = delta & 0xff
        return bytes(self.code)


def program():
    a = Asm(0xf000)
    lda_imm = lambda v: a.op(0xa9, v, 1)
    ldx_imm = lambda v: a.op(0xa2, v, 1)
    ldy_imm = lambda v: a.op(0xa0, v, 1)
    lda_abs = lambda v: a.op(0xad, v, 2)
    sta_abs = lambda v: a.op(0x8d, v, 2)
    sta_zp = lambda v: a.op(0x85, v, 1)
    and_imm = lambda v: a.op(0x29, v, 1)
    eor_imm = lambda v: a.op(0x49, v, 1)
    jsr = lambda v: a.op(0x20, v, 2)

    # Show A's top bit at ADDR: a block when it is set.
    def show_bit(addr):
        and_imm(0x80)
        eor_imm(SPACE)        # $80 -> $20 (block), $00 -> $A0 (space)
        sta_abs(addr)

    # Show A as two hex digits at ADDR.
    def show_hex(addr):
        a.op(0x48)                    # PHA
        lda_imm(addr & 0xff)
        sta_zp(0x02)
        lda_imm(addr >> 8)
        sta_zp(0x03)
        a.op(0x68)                    # PLA
        jsr('hex')

    a.label('reset')
    a.op(0x78)                        # SEI
    a.op(0xd8)                        # CLD
    ldx_imm(0xff)
    a.op(0x9a)                        # TXS
    for switch in (0xc000, 0xc00c, 0xc00e):   # 80STORE, 80COL, ALTCHAR off
        sta_abs(switch)
    lda_abs(0xc051)                   # text
    lda_abs(0xc054)                   # page 1
    lda_imm(SPACE)
    ldx_imm(0)
    a.label('clear')
    for page in (0x400, 0x500, 0x600, 0x700):
        a.op(0x9d, page, 2)           # STA page,X
    a.op(0xe8)                        # INX
    a.branch(0xd0, 'clear')           # BNE
    lda_imm(0)
    sta_zp(0x00)

    a.label('loop')
    # The start of the next vertical blank (on a IIe, $C019's top bit is
    # set outside it).
    a.label('wait_display')
    lda_abs(0xc019)
    a.branch(0x10, 'wait_display')    # BPL: in the blank still
    a.label('wait_blank')
    lda_abs(0xc019)
    a.branch(0x30, 'wait_blank')      # BMI: not yet

    a.op(0xe6, 0x00, 1)               # INC frames
    a.op(0xa5, 0x00, 1)               # LDA frames
    show_hex(ROW0)

    lda_abs(0xc010)                   # any key down (and clear the strobe)
    show_bit(ROW6)
    lda_abs(0xc061)
    show_bit(ROW6 + 2)
    lda_abs(0xc062)
    show_bit(ROW6 + 4)

    for paddle in (0, 1):
        ldx_imm(paddle)
        jsr('pread')
        a.op(0x98)                    # TYA
        show_hex(ROW7 + 3 * paddle)

    # A buzz: the speaker clicked 32 times.
    ldx_imm(32)
    a.label('click')
    lda_abs(0xc030)
    ldy_imm(40)
    a.label('click_wait')
    a.op(0x88)                        # DEY
    a.branch(0xd0, 'click_wait')
    a.op(0xca)                        # DEX
    a.branch(0xd0, 'click')
    a.op(0x4c, 'loop', 2)             # JMP

    # Paddle X into Y, 0-255: the Apple II ROM's PREAD.
    a.label('pread')
    lda_abs(0xc070)
    ldy_imm(0)
    a.op(0xea)                        # NOP
    a.op(0xea)
    a.label('pread_loop')
    a.op(0xbd, 0xc064, 2)             # LDA $C064,X
    a.branch(0x10, 'pread_done')      # BPL
    a.op(0xc8)                        # INY
    a.branch(0xd0, 'pread_loop')      # BNE
    a.op(0x88)                        # DEY
    a.label('pread_done')
    a.op(0x60)                        # RTS

    # A as two hex digits at ($02).
    a.label('hex')
    a.op(0x48)                        # PHA
    for _ in range(4):
        a.op(0x4a)                    # LSR A
    a.op(0xaa)                        # TAX
    a.op(0xbd, 'digits', 2)           # LDA digits,X
    ldy_imm(0)
    a.op(0x91, 0x02, 1)               # STA ($02),Y
    a.op(0x68)                        # PLA
    and_imm(0x0f)
    a.op(0xaa)                        # TAX
    a.op(0xbd, 'digits', 2)
    a.op(0xc8)                        # INY
    a.op(0x91, 0x02, 1)
    a.op(0x60)                        # RTS

    a.label('digits')
    a.byte(*(ord(c) | 0x80 for c in '0123456789ABCDEF'))

    a.label('interrupt')
    a.op(0x40)                        # RTI
    return a


def rom():
    a = program()
    code = a.finish()
    image = bytearray(32 * 1024)
    # $C000-$FFFF is the last 16K of the file.
    start = 0x4000 + (a.origin - 0xc000)
    image[start:start + len(code)] = code
    vectors = 0x4000 + (0xfffa - 0xc000)
    for i, name in enumerate(('interrupt', 'reset', 'interrupt')):
        address = a.labels[name]
        image[vectors + 2 * i:vectors + 2 * i + 2] = bytes((address & 0xff, address >> 8))
    return bytes(image)


out = sys.argv[1] if len(sys.argv) > 1 else '.'
with open(out + '/ROM', 'wb') as f:
    f.write(rom())
print('wrote ' + out + '/ROM')
