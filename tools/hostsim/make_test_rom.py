#!/usr/bin/env python3
#
# Makes stand-in CoCo ROMs for the host simulator's tests.
#
# The real Color BASIC ROMs are copyrighted, so the tests can't use them.
# This writes an 8K "bas13.rom" (Color BASIC 1.3's name, which XRoar looks
# for) holding a small 6809 program, and an empty "extbas11.rom", into the
# directory given. The program:
#
#   - prints "BMCOCO TEST ROM" on the 32x16 text screen,
#   - shows the keyboard matrix on row 10 (one character per column, which
#     changes while a key in that column is down),
#   - shows the right joystick's X and Y (as read with the 6 bit DAC and
#     the comparator) as characters on row 12, and the buttons,
#   - counts frames in the top right corner,
#   - toggles the single bit sound output every frame (a 30 Hz buzz).
#
#   python3 make_test_rom.py DIR
#

import sys


class Asm:
    def __init__(self, origin):
        self.origin = origin
        self.code = bytearray()
        self.labels = {}
        self.fixups = []  # (offset, label, kind)

    @property
    def pc(self):
        return self.origin + len(self.code)

    def label(self, name):
        self.labels[name] = self.pc

    def emit(self, *data):
        for d in data:
            self.code.append(d & 0xff)

    def word(self, value):
        self.emit(value >> 8, value)

    def ref16(self, label):
        self.fixups.append((len(self.code), label, 'abs'))
        self.word(0)

    def branch(self, opcode, label):
        self.emit(opcode)
        self.fixups.append((len(self.code), label, 'rel8'))
        self.emit(0)

    def resolve(self):
        for offset, label, kind in self.fixups:
            target = self.labels[label]
            if kind == 'abs':
                self.code[offset] = target >> 8
                self.code[offset + 1] = target & 0xff
            else:
                rel = target - (self.origin + offset + 1)
                if not -128 <= rel <= 127:
                    raise ValueError('branch to %s out of range' % label)
                self.code[offset] = rel & 0xff


SCREEN = 0x0400

a = Asm(0xA000)
a.label('start')
a.emit(0x1A, 0x50)                    # ORCC #$50      no interrupts
a.emit(0x10, 0xCE); a.word(0x0F00)    # LDS #$0F00    (the SAM starts in 4K mode)
# PIA0: port A inputs (keyboard rows, comparator), port B outputs (columns)
a.emit(0x7F); a.word(0xFF01)          # CLR $FF01      CRA: DDR
a.emit(0x7F); a.word(0xFF00)          # CLR $FF00      DDRA = inputs
a.emit(0x86, 0x04)                    # LDA #$04
a.emit(0xB7); a.word(0xFF01)          # STA $FF01      CRA: data
a.emit(0x7F); a.word(0xFF03)          # CLR $FF03      CRB: DDR
a.emit(0x86, 0xFF)                    # LDA #$FF
a.emit(0xB7); a.word(0xFF02)          # STA $FF02      DDRB = outputs
a.emit(0x86, 0x04)                    # LDA #$04
a.emit(0xB7); a.word(0xFF03)          # STA $FF03      CRB: data
# PIA1: port A bits 2-7 DAC out, bit 1 printer; port B: VDG and sound
a.emit(0x7F); a.word(0xFF21)          # CLR $FF21
a.emit(0x86, 0xFE)                    # LDA #$FE
a.emit(0xB7); a.word(0xFF20)          # STA $FF20      DDRA
a.emit(0x86, 0x04)                    # LDA #$04
a.emit(0xB7); a.word(0xFF21)          # STA $FF21
a.emit(0x7F); a.word(0xFF23)          # CLR $FF23
a.emit(0x86, 0xFA)                    # LDA #$FA       VDG bits 3-7, sound bit 1
a.emit(0xB7); a.word(0xFF22)          # STA $FF22      DDRB
a.emit(0x86, 0x04)                    # LDA #$04
a.emit(0xB7); a.word(0xFF23)          # STA $FF23
a.emit(0x7F); a.word(0xFF22)          # CLR $FF22      alphanumeric, CSS 0
# SAM: display from $0400 (F1 set)
a.emit(0xB7); a.word(0xFFC9)          # STA $FFC9
# Clear the screen to (green) spaces
a.emit(0x8E); a.word(SCREEN)          # LDX #$0400
a.emit(0x86, 0x60)                    # LDA #$60
a.label('clear')
a.emit(0xA7, 0x80)                    # STA ,X+
a.emit(0x8C); a.word(SCREEN + 512)    # CMPX #$0600
a.branch(0x26, 'clear')               # BNE clear
# The message on row 2
a.emit(0x8E); a.ref16('msg')          # LDX #msg
a.emit(0x10, 0x8E); a.word(SCREEN + 2 * 32 + 8)  # LDY
a.label('print')
a.emit(0xA6, 0x80)                    # LDA ,X+
a.branch(0x27, 'loop')                # BEQ loop
a.emit(0xA7, 0xA0)                    # STA ,Y+
a.branch(0x20, 'print')               # BRA print

a.label('loop')
# Keyboard: one column at a time, rows as a character
a.emit(0xC6, 0xFE)                    # LDB #$FE
a.emit(0x8E); a.word(SCREEN + 10 * 32 + 12)  # LDX
a.label('col')
a.emit(0xF7); a.word(0xFF02)          # STB $FF02
a.emit(0xB6); a.word(0xFF00)          # LDA $FF00
a.emit(0x84, 0x7F)                    # ANDA #$7F
a.emit(0xA7, 0x80)                    # STA ,X+
a.emit(0x1A, 0x01)                    # ORCC #$01      carry in
a.emit(0x59)                          # ROLB
a.emit(0xC1, 0xFF)                    # CMPB #$FF
a.branch(0x26, 'col')                 # BNE col
a.emit(0x86, 0xFF)                    # LDA #$FF
a.emit(0xB7); a.word(0xFF02)          # STA $FF02      no column

# Joystick buttons: rows 0 and 1 with no column selected (active low)
a.emit(0xB6); a.word(0xFF00)          # LDA $FF00
a.emit(0x84, 0x03)                    # ANDA #$03
a.emit(0x8B, 0x30)                    # ADDA #'0'      '0'-'3'
a.emit(0xB7); a.word(SCREEN + 12 * 32 + 20)

# Joystick: right X (mux 0) and Y (mux 1). Successive approximation with
# the 6 bit DAC: $FF20 bits 2-7, comparator in $FF00 bit 7.
for mux, column in ((0, 12), (1, 16)):
    # Select the axis: PIA0 CRA bit 3 (SEL1, low bit), CRB bit 3 (SEL2)
    a.emit(0x86, 0x3C if mux else 0x34)   # LDA: CA2 out, high/low; data
    a.emit(0xB7); a.word(0xFF01)          # STA $FF01
    a.emit(0x86, 0x34)                    # LDA #$34      SEL2 low
    a.emit(0xB7); a.word(0xFF03)          # STA $FF03
    a.emit(0xC6, 0x00)                    # LDB #0        result
    a.emit(0x86, 0x80)                    # LDA #$80      trial bit
    a.label('bit%d' % mux)
    a.emit(0x34, 0x02)                    # PSHS A
    a.emit(0x34, 0x04)                    # PSHS B
    a.emit(0xAA, 0xE4)                    # ORA ,S        trial = B | bit
    a.emit(0x34, 0x02)                    # PSHS A
    a.emit(0x84, 0xFC)                    # ANDA #$FC
    a.emit(0xB7); a.word(0xFF20)          # STA $FF20     DAC
    a.emit(0x12, 0x12, 0x12)              # NOP x3        settle
    a.emit(0xF6); a.word(0xFF00)          # LDB $FF00
    a.emit(0x35, 0x02)                    # PULS A        trial
    a.emit(0x5D)                          # TSTB          bit 7: joystick >= DAC
    a.emit(0x35, 0x04)                    # PULS B        result
    a.branch(0x2A, 'skip%d' % mux)        # BPL skip      below: keep result
    a.emit(0x1F, 0x89)                    # TFR A,B       result = trial
    a.label('skip%d' % mux)
    a.emit(0x35, 0x02)                    # PULS A        bit
    a.emit(0x44)                          # LSRA
    a.emit(0x81, 0x02)                    # CMPA #$02
    a.branch(0x26, 'bit%d' % mux)         # BNE bit       6 bits: $80..$04
    # Show B>>2 (0-63) as a character
    a.emit(0x54)                          # LSRB
    a.emit(0x54)                          # LSRB
    a.emit(0xF7); a.word(SCREEN + 12 * 32 + column)
a.emit(0x86, 0x34)                        # LDA #$34      SEL1 low again
a.emit(0xB7); a.word(0xFF01)
a.emit(0x86, 0x04)                        # LDA #$04
a.emit(0xB7); a.word(0xFF03)              # CRB: back to plain data

# Wait for field sync (PIA0 CB1: CRB bit 7), then clear the flag
a.label('wait')
a.emit(0xB6); a.word(0xFF03)          # LDA $FF03
a.branch(0x2A, 'wait')                # BPL wait
a.emit(0xB6); a.word(0xFF02)          # LDA $FF02
a.emit(0x7C); a.word(SCREEN + 31)     # INC $041F      frame counter
a.emit(0xB6); a.word(0xFF22)          # LDA $FF22
a.emit(0x88, 0x02)                    # EORA #$02      sound bit
a.emit(0xB7); a.word(0xFF22)          # STA $FF22
a.emit(0x7E); a.ref16('loop')         # JMP loop

a.label('msg')
for c in 'BMCOCO TEST ROM':
    a.emit(ord(c) if c != ' ' else 0x60)
a.emit(0)

a.resolve()

rom = bytearray([0xFF] * 0x2000)
rom[:len(a.code)] = a.code
# Vectors: $FFF0-$FFFF read from $BFF0-$BFFF
for vec in range(0x1FF2, 0x2000, 2):
    rom[vec] = 0xA0
    rom[vec + 1] = 0x00

out = sys.argv[1] if len(sys.argv) > 1 else '.'
with open(out + '/bas13.rom', 'wb') as f:
    f.write(rom)
with open(out + '/extbas11.rom', 'wb') as f:
    f.write(bytes([0xFF] * 0x2000))
print('%d bytes of code' % len(a.code))
