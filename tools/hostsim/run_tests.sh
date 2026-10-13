#!/bin/bash
#
# Runs the emulator port on the PC (see host_sim.c) through a set of
# scripted tests and checks the results.
#
#   tools/hostsim/run_tests.sh
#   BMAPPLE2_ROMS=/path/to/roms tools/hostsim/run_tests.sh
#
# The tests run a small stand-in ROM (make_test_rom.py), which KEGS runs as
# an Apple IIe, since the real ROMs can't be shipped. With BMAPPLE2_ROMS set
# to a folder holding a real IIgs ROM (ROM.01 or ROM.03, as in the SD card's
# roms folder), the IIgs is tested too. Output, including screenshots
# (.ppm), goes to tools/hostsim/out/.
#

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$HERE/out
SIM=$HERE/build/bmapple2_sim

make -s -C "$HERE" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)" || exit 1

rm -rf "$OUT"
mkdir -p "$OUT/testroms"
python3 "$HERE/make_test_rom.py" "$OUT/testroms" > /dev/null || exit 1

FAILED=0
pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1: $2"; FAILED=$((FAILED + 1)); }

# run NAME REFRESH ROMS CMDLINE SCRIPT: runs the simulator in out/NAME, a
# stand-in SD card with ROMS as its roms folder.
run() {
    local name=$1 refresh=$2 roms=$3 cmdline=$4 script=$5
    mkdir -p "$OUT/$name"
    ln -s "$roms" "$OUT/$name/roms"
    (cd "$OUT/$name" && printf "$script" | BMAPPLE2_REFRESH=$refresh \
        BMAPPLE2_CMDLINE="root=. $cmdline" "$SIM" > log.txt 2>&1)
}

# check_log NAME REGEX MESSAGE: the log must contain REGEX
check_log() {
    if grep -Eq "$2" "$OUT/$1/log.txt"; then
        pass "$1: $3"
    else
        fail "$1" "$3 (see out/$1/log.txt)"
    fi
}

# no_underruns NAME: the sound output never ran dry
no_underruns() {
    if grep -q "audio underrun" "$OUT/$1/log.txt"; then
        fail "$1" "audio underrun (see out/$1/log.txt)"
    else
        pass "$1: no audio underruns"
    fi
}

# check_audio NAME: no underruns, sound was made, every frame presented
check_audio() {
    local log=$OUT/$1/log.txt line frame presented changes
    no_underruns "$1"
    line=$(grep "frames presented" "$log" | tail -1)
    frame=$(echo "$line" | sed 's/.*frame \([0-9]*\):.*/\1/')
    presented=$(echo "$line" | sed 's/.*: \([0-9]*\) frames presented/\1/')
    if [ -n "$frame" ] && [ "${presented:-0}" -ge "$frame" ]; then
        pass "$1: no dropped frames ($presented/$frame)"
    else
        fail "$1" "frames presented: $line"
    fi
    changes=$(grep "changing samples" "$log" | tail -1 | sed 's/.*changing samples \([0-9]*\).*/\1/')
    if [ "${changes:-0}" -gt 0 ]; then
        pass "$1: sound output"
    else
        fail "$1" "no sound reached the output"
    fi
}

# same NAME A B / differ NAME A B: compare two screenshots of out/NAME,
# leaving out the top 100 lines, where the test ROM counts frames.
screens_equal() {
    python3 - "$1" "$2" <<'EOF'
import sys
def body(path):
    data = open(path, 'rb').read()
    fields, pos = [], 0
    while len(fields) < 4:
        while data[pos:pos + 1].isspace():
            pos += 1
        end = pos
        while not data[end:end + 1].isspace():
            end += 1
        fields.append(data[pos:end])
        pos = end
    width = int(fields[1])
    return data[pos + 1 + 100 * width * 3:]
sys.exit(0 if body(sys.argv[1]) == body(sys.argv[2]) else 1)
EOF
}
same() {
    if [ -f "$OUT/$1/$2.ppm" ] && [ -f "$OUT/$1/$3.ppm" ] && screens_equal "$OUT/$1/$2.ppm" "$OUT/$1/$3.ppm"; then
        pass "$1: $2 and $3 are the same"
    else
        fail "$1" "$2 and $3 differ"
    fi
}
differ() {
    if [ -f "$OUT/$1/$2.ppm" ] && [ -f "$OUT/$1/$3.ppm" ] && ! screens_equal "$OUT/$1/$2.ppm" "$OUT/$1/$3.ppm"; then
        pass "$1: $2 and $3 differ"
    else
        fail "$1" "$2 and $3 should differ"
    fi
}

# has_colour NAME FILE R G B MESSAGE: the screenshot has a pixel of that colour
has_colour() {
    if python3 - "$OUT/$1/$2.ppm" "$3" "$4" "$5" <<'EOF'
import sys
data = open(sys.argv[1], 'rb').read()
fields, pos = [], 0
while len(fields) < 4:
    while data[pos:pos + 1].isspace():
        pos += 1
    end = pos
    while not data[end:end + 1].isspace():
        end += 1
    fields.append(data[pos:end])
    pos = end
pixels = data[pos + 1:]
want = bytes(int(v) for v in sys.argv[2:5])
sys.exit(0 if any(pixels[i:i + 3] == want for i in range(0, len(pixels), 3)) else 1)
EOF
    then
        pass "$1: $6"
    else
        fail "$1" "$6 (no $3,$4,$5 pixel in $2.ppm)"
    fi
}

TESTROMS=$OUT/testroms

# USB HID usages used below.
F10=0x43 DOWN=0x51 ENTER=0x28 POWER=0x66

echo "Boot, locked to a 60 Hz display"
run boot 60 "$TESTROMS" "" '120 shot a.ppm\n121 shot b.ppm\n600 audio\n601 quit\n'
check_log boot "IIgs 59\.9[0-9] Hz, synchronised to the display" "locked to 60 Hz"
check_log boot "created .*config\.kegs" "a new card gets a config.kegs"
check_log boot "Read: 32768 bytes of ROM" "the stand-in IIe ROM was loaded"
check_audio boot
if cmp -s "$OUT/boot/a.ppm" "$OUT/boot/b.ppm"; then
    fail boot "the frame counter didn't move from one frame to the next"
else
    pass "boot: a new frame every display refresh"
fi

echo "On a 50 Hz display"
run timer 50 "$TESTROMS" "" '600 audio\n601 quit\n'
check_log timer "timer paced" "falls back to timer pacing"
no_underruns timer

echo "Keyboard"
run keys 60 "$TESTROMS" "" '120 shot idle.ppm\n130 key 0x04 1\n140 shot a.ppm\n141 key 0x04 0\n150 shot idle2.ppm\n151 quit\n'
differ keys idle a
same keys idle idle2
# Gamepad X is Space.
run padkeys 60 "$TESTROMS" "" '120 pad 0 0\n125 shot idle.ppm\n130 pad 0 0x0004\n140 shot x.ppm\n141 pad 0 0\n150 shot idle2.ppm\n151 quit\n'
differ padkeys idle x
same padkeys idle idle2

echo "Joysticks: analog stick, D-pad, GPIO"
# Full left on the stick, the D-pad and a GPIO joystick all read paddle 0
# as 0; half way on the stick reads in between. Fire is button 0.
run joy 60 "$TESTROMS" "" '120 pad 0 0\n120 axis 0 32768 32768\n140 shot centre.ppm\n150 axis 0 0 32768\n170 shot stick.ppm\n175 axis 0 16384 32768\n190 shot half.ppm\n200 axis 0 32768 32768\n200 pad 0 0x4000\n220 shot dpad.ppm\n230 pad 0 0\n230 gpio 0 0x04\n250 shot gpio.ppm\n260 gpio 0 0x10\n280 shot fire.ppm\n290 gpio 0 0\n310 shot centre2.ppm\n311 quit\n'
differ joy centre stick
differ joy stick half
differ joy centre half
same joy stick dpad
same joy dpad gpio
differ joy centre fire
same joy centre centre2

echo "Menu (F10)"
# Down three times (past Reset and Restart) to Video, which opens.
run menu 60 "$TESTROMS" "" "120 shot before.ppm\n130 key $F10 1\n131 key $F10 0\n150 shot menu.ppm\n160 key $DOWN 1\n161 key $DOWN 0\n162 key $DOWN 1\n163 key $DOWN 0\n164 key $DOWN 1\n165 key $DOWN 0\n170 key $ENTER 1\n171 key $ENTER 0\n190 shot video.ppm\n200 key $F10 1\n201 key $F10 0\n230 shot after.ppm\n231 quit\n"
has_colour menu menu 255 129 0 "the menu shows its selection bar"
differ menu menu video
same menu before after

echo "Gamepad opens the menu"
run padmenu 60 "$TESTROMS" "" '120 pad 0 0\n130 pad 0 0x0100\n135 pad 0 0\n150 shot menu.ppm\n151 quit\n'
has_colour padmenu menu 255 129 0 "Home opens the menu"

echo "The power key (Pi 400: Fn+F10) shuts down"
run powerkey 60 "$TESTROMS" "" "150 key $POWER 1\n152 key $POWER 0\n250 quit\n"
check_log powerkey "Shutting down" "the power key while playing"
check_log powerkey "power off requested" "the power key powers off"
run powerkeymenu 60 "$TESTROMS" "" "150 key $F10 1\n152 key $F10 0\n180 key $POWER 1\n182 key $POWER 0\n250 quit\n"
check_log powerkeymenu "power off requested" "the power key in the menu powers off"

echo "Web UI requests"
run reboot 60 "$TESTROMS" "" '120 webui reboot\n200 quit\n'
check_log reboot "reboot requested" "reboots"
run coldstart 60 "$TESTROMS" "" '120 webui coldstart\n200 quit\n'
check_log coldstart "Web UI: restart" "restarts the machine"

echo "Web UI starts disk images from the right slot"
mkdir -p "$OUT/disks"
python3 - "$OUT/disks" <<'EOF'
import sys
d = sys.argv[1]
for name, size in (('game.dsk', 143360), ('disk.po', 819200), ('hard.hdv', 4 * 1024 * 1024)):
    with open(d + '/' + name, 'wb') as f:
        f.truncate(size)
# A WOZ 2 header and the start of its INFO chunk: disk type 2, a 3.5" disk.
woz = bytearray(b'WOZ2\xff\n\r\n' + bytes(4) + b'INFO' + (60).to_bytes(4, 'little') + bytes(60))
woz[21] = 2
open(d + '/disk35.woz', 'wb').write(woz)
EOF
run start 60 "$TESTROMS" "" "120 run $OUT/disks/game.dsk\n200 run $OUT/disks/disk.po\n280 run $OUT/disks/hard.hdv\n360 run $OUT/disks/disk35.woz\n440 quit\n"
check_log start "starting .*game\.dsk from slot 6" "a 5.25\" disk goes in slot 6"
check_log start "starting .*disk\.po from slot 5" "an 800K disk goes in slot 5"
check_log start "starting .*hard\.hdv from slot 7" "a hard disk goes in slot 7"
check_log start "starting .*disk35\.woz from slot 5" "a 3.5\" WOZ disk goes in slot 5"
check_log start "Inserting disk .*hard\.hdv.* in slot 7" "KEGS mounts it"

echo "ROMs made from MAME's ROM chips"
# Stand-in chips: the IIgs ROM 3's 341-0728 and 341-0748 (each half of the
# second marked), and the IIe's three. Only the arrangement is checked.
mkdir -p "$OUT/mameroms"
python3 - "$OUT/mameroms" <<'EOF'
import sys
d = sys.argv[1] + '/'
open(d + '341-0728', 'wb').write(b'\x28' * 0x20000)
open(d + '341-0748', 'wb').write(b'\x48' * 0x10000 + b'\x49' * 0x10000)
open(d + '342-0304-a.e10', 'wb').write(b'\xcd' * 0x2000)
open(d + '342-0303-a.e8', 'wb').write(b'\xef' * 0x2000)
open(d + '341-0027-a.p5', 'wb').write(b'\x27' * 0x100)
EOF
run mame 60 "$OUT/mameroms" "" '100 quit\n'
check_log mame "made .*roms/ROM\.03" "ROM.03 made"
check_log mame "made .*roms/APPLE2E\.ROM" "APPLE2E.ROM made"
if python3 - "$OUT/mameroms" <<'EOF'
import sys
d = sys.argv[1] + '/'
gs = open(d + 'ROM.03', 'rb').read()
e = open(d + 'APPLE2E.ROM', 'rb').read()
ok = (gs == b'\x28' * 0x20000 + b'\x49' * 0x10000 + b'\x48' * 0x10000 and
      e == bytes(0x600) + b'\x27' * 0x100 + bytes(0x4000 - 0x700) + b'\xcd' * 0x2000 + b'\xef' * 0x2000)
sys.exit(0 if ok else 1)
EOF
then
    pass "mame: the chips are where KEGS expects them"
else
    fail mame "ROM.03 or APPLE2E.ROM is laid out wrong"
fi

echo "Settings are saved"
# Menu, Video, CRT Effect (on), close.
run settings 60 "$TESTROMS" "" "120 key $F10 1\n121 key $F10 0\n130 key $DOWN 1\n131 key $DOWN 0\n132 key $DOWN 1\n133 key $DOWN 0\n134 key $DOWN 1\n135 key $DOWN 0\n140 key $ENTER 1\n141 key $ENTER 0\n150 key $DOWN 1\n151 key $DOWN 0\n160 key $ENTER 1\n161 key $ENTER 0\n170 key $F10 1\n171 key $F10 0\n200 quit\n"
if grep -q "^crt_shader=1" "$OUT/settings/bmapple2.cfg" 2>/dev/null; then
    pass "settings: Video > CRT Effect saved to bmapple2.cfg"
else
    fail settings "crt_shader=1 not in bmapple2.cfg"
fi

if [ -n "${BMAPPLE2_ROMS:-}" ]; then
    echo "Apple IIgs (real ROM from $BMAPPLE2_ROMS)"
    run iigs 60 "$BMAPPLE2_ROMS" "" '300 shot boot.ppm\n310 type PRINT "HELLO"\\n\n500 shot hello.ppm\n600 audio\n601 quit\n'
    check_log iigs "Read: (131072|262144) bytes of ROM" "IIgs ROM found"
    check_log iigs "IIgs 59\.9[0-9] Hz, synchronised to the display" "IIgs locked to 60 Hz"
    differ iigs boot hello
    no_underruns iigs
fi

echo
if [ $FAILED -eq 0 ]; then
    echo "All tests passed."
else
    echo "$FAILED test(s) failed."
fi
exit $((FAILED > 0))
