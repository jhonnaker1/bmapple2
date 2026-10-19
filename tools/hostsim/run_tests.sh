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

echo "Sockets"
# On the Pi the C library's socket calls hold its open files' lock while
# they wait, and can't be made non-blocking: the port (and its copy of
# KEGS's modem driver) must use host_tcp_* instead.
if grep -nE '(^|[^_a-zA-Z])(connect|recv|send|socket|accept|bind|listen|gethostbyname|getaddrinfo)\(' "$HERE/../../src/port/"*.c > /dev/null; then
    fail sockets "src/port calls the C library's sockets (use host_tcp_*)"
else
    pass "sockets: the port uses host_tcp_*, not the C library's sockets"
fi
if grep -q "scc_socket_driver.c" "$HERE/../../src/port/kegs.mk" && \
   grep -A40 "^KEGS_SRCS" "$HERE/../../src/port/kegs.mk" | grep -q "scc_socket_driver.c"; then
    fail sockets "KEGS's own scc_socket_driver.c is built (src/port's copy replaces it)"
else
    pass "sockets: KEGS's scc_socket_driver.c is replaced by src/port's"
fi

echo "Boot, locked to a 60 Hz display"
run boot 60 "$TESTROMS" "" '120 shot a.ppm\n121 shot b.ppm\n600 audio\n601 quit\n'
check_log boot "machine 59\.9[0-9] Hz, synchronised to the display" "locked to 60 Hz"
check_log boot "created .*config\.kegs" "a new card gets a config.kegs"
check_log boot "Read: 32768 bytes of ROM" "the stand-in IIe ROM was loaded"
check_audio boot
if cmp -s "$OUT/boot/a.ppm" "$OUT/boot/b.ppm"; then
    fail boot "the frame counter didn't move from one frame to the next"
else
    pass "boot: a new frame every display refresh"
fi

echo "Sound on a 60 Hz display, at 44.1 and 48 kHz"
# The IIe's 59.92 Hz locked to 60 Hz runs 0.13% fast: the sound must be
# slowed to fit, never cut off with the buffer full (which clicks), at the
# VideoCore's 44.1 kHz and the Pi 4 and 5's 48 kHz. (Dropping a frame of
# sound now and then filled it within a minute.)
for rate in 44100 48000; do
    BMAPPLE2_SIM_RATE=$rate run sound$rate 60 "$TESTROMS" "" '2400 audio\n2401 quit\n'
    line=$(grep "audio written" "$OUT/sound$rate/log.txt" | tail -1)
    max=$(echo "$line" | sed 's/.*after write max \([0-9]*\).*/\1/')
    cap=$(grep "sound open" "$OUT/sound$rate/log.txt" | head -1 | sed 's/.*, \([0-9]*\) frames.*/\1/')
    if [ -n "$max" ] && [ -n "$cap" ] && [ "$max" -le $((cap * 90 / 100)) ]; then
        pass "sound$rate: the sound buffer never fills ($max of $cap)"
    else
        fail sound$rate "the sound buffer filled: $line (capacity $cap)"
    fi
    check_audio sound$rate
done

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
# Down four times (past Reset, Restart and Machine) to Video, which opens.
run menu 60 "$TESTROMS" "" "120 shot before.ppm\n130 key $F10 1\n131 key $F10 0\n150 shot menu.ppm\n160 key $DOWN 1\n161 key $DOWN 0\n162 key $DOWN 1\n163 key $DOWN 0\n164 key $DOWN 1\n165 key $DOWN 0\n166 key $DOWN 1\n167 key $DOWN 0\n170 key $ENTER 1\n171 key $ENTER 0\n190 shot video.ppm\n200 key $F10 1\n201 key $F10 0\n230 shot after.ppm\n231 quit\n"
has_colour menu menu 255 129 0 "the menu shows its selection bar"
differ menu menu video
same menu before after

echo "Screen size"
# Menu, Video, Screen Size, down two from 100% to 90%: saved as scale=0.90
# (for a TV that cuts off the edges).
run size 60 "$TESTROMS" "" "130 key $F10 1\n131 key $F10 0\n140 key $DOWN 1\n141 key $DOWN 0\n142 key $DOWN 1\n143 key $DOWN 0\n144 key $DOWN 1\n145 key $DOWN 0\n146 key $DOWN 1\n147 key $DOWN 0\n150 key $ENTER 1\n151 key $ENTER 0\n160 key $DOWN 1\n161 key $DOWN 0\n170 key $ENTER 1\n171 key $ENTER 0\n180 key $DOWN 1\n181 key $DOWN 0\n182 key $DOWN 1\n183 key $DOWN 0\n190 key $ENTER 1\n191 key $ENTER 0\n200 shot video.ppm\n201 quit\n"
if grep -q "^scale=0.90" "$OUT/size/bmapple2.cfg" 2>/dev/null; then
    pass "size: Video > Screen Size 90% saved as scale=0.90"
else
    fail size "scale=0.90 not in bmapple2.cfg (see out/size/log.txt)"
fi

echo "GPIO joysticks in the menu"
# Down four times and fire on GPIO joystick 1: the same page as Down and
# Enter on the keyboard (Video).
run gpiomenu 60 "$TESTROMS" "gpio_joysticks=1" "130 key $F10 1\n131 key $F10 0\n136 shot menu.ppm\n140 gpio 0 0x02\n142 gpio 0 0\n146 gpio 0 0x02\n148 gpio 0 0\n152 gpio 0 0x02\n154 gpio 0 0\n158 gpio 0 0x02\n160 gpio 0 0\n170 gpio 0 0x10\n172 gpio 0 0\n190 shot gpio.ppm\n191 quit\n"
run keymenu 60 "$TESTROMS" "gpio_joysticks=1" "130 key $F10 1\n131 key $F10 0\n140 key $DOWN 1\n141 key $DOWN 0\n146 key $DOWN 1\n147 key $DOWN 0\n152 key $DOWN 1\n153 key $DOWN 0\n158 key $DOWN 1\n159 key $DOWN 0\n170 key $ENTER 1\n171 key $ENTER 0\n190 shot keys.ppm\n191 quit\n"
cp "$OUT/keymenu/keys.ppm" "$OUT/gpiomenu/keys.ppm" 2>/dev/null
differ gpiomenu menu gpio
same gpiomenu gpio keys

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
# Menu, Video, CRT Effect (on, three down past Monitor, Screen Size and
# Frame Lock), close.
run settings 60 "$TESTROMS" "" "120 key $F10 1\n121 key $F10 0\n130 key $DOWN 1\n131 key $DOWN 0\n132 key $DOWN 1\n133 key $DOWN 0\n134 key $DOWN 1\n135 key $DOWN 0\n136 key $DOWN 1\n137 key $DOWN 0\n140 key $ENTER 1\n141 key $ENTER 0\n150 key $DOWN 1\n151 key $DOWN 0\n152 key $DOWN 1\n153 key $DOWN 0\n154 key $DOWN 1\n155 key $DOWN 0\n160 key $ENTER 1\n161 key $ENTER 0\n170 key $F10 1\n171 key $F10 0\n200 quit\n"
if grep -q "^crt_shader=1" "$OUT/settings/bmapple2.cfg" 2>/dev/null; then
    pass "settings: Video > CRT Effect saved to bmapple2.cfg"
else
    fail settings "crt_shader=1 not in bmapple2.cfg"
fi

echo "Monitors"
# Every lit pixel of a green monitor is green (red and blue the same, and
# less than green); amber is red, then green, no blue; white is grey.
run green 60 "$TESTROMS" "monitor=green" '120 shot a.ppm\n121 quit\n'
run amber 60 "$TESTROMS" "monitor=amber" '120 shot a.ppm\n121 quit\n'
run white 60 "$TESTROMS" "monitor=white" '120 shot a.ppm\n121 quit\n'
for m in green amber white; do
    if python3 - "$OUT/$m/a.ppm" "$m" <<'EOF'
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
px = data[pos + 1:]
lit = 0
for i in range(0, len(px), 3):
    r, g, b = px[i], px[i + 1], px[i + 2]
    if r + g + b == 0:
        continue
    lit += 1
    if sys.argv[2] == 'green' and not (abs(r - b) <= 8 and g > r):
        sys.exit(1)
    if sys.argv[2] == 'amber' and not (r >= g > b and b <= 8):
        sys.exit(1)
    if sys.argv[2] == 'white' and not (abs(r - g) <= 8 and abs(g - b) <= 8):
        sys.exit(1)
sys.exit(0 if lit > 100 else 1)
EOF
    then
        pass "$m: the picture is all $m"
    else
        fail "$m" "pixels that aren't $m in out/$m/a.ppm"
    fi
done
# The monitor is chosen in the menu (Video, Monitor, Green) and kept.
run monitormenu 60 "$TESTROMS" "" "120 key $F10 1\n121 key $F10 0\n130 key $DOWN 1\n131 key $DOWN 0\n132 key $DOWN 1\n133 key $DOWN 0\n134 key $DOWN 1\n135 key $DOWN 0\n136 key $DOWN 1\n137 key $DOWN 0\n140 key $ENTER 1\n141 key $ENTER 0\n150 key $ENTER 1\n151 key $ENTER 0\n160 key $DOWN 1\n161 key $DOWN 0\n162 key $DOWN 1\n163 key $DOWN 0\n170 key $ENTER 1\n171 key $ENTER 0\n180 key $F10 1\n181 key $F10 0\n200 quit\n"
if grep -q "^monitor=green" "$OUT/monitormenu/bmapple2.cfg" 2>/dev/null; then
    pass "monitormenu: Video > Monitor > Green saved to bmapple2.cfg"
else
    fail monitormenu "monitor=green not in bmapple2.cfg"
fi

echo "Volume"
run volume0 60 "$TESTROMS" "volume=0" '600 audio\n601 quit\n'
check_log volume0 "changing samples 0" "volume=0 is silent"
check_log boot "changing samples [1-9]" "the default volume isn't"

echo "Machine choice"
# With only the IIe's ROM, machine=iigs3 falls back to the IIe.
run machine 60 "$TESTROMS" "machine=iigs3" '100 quit\n'
check_log machine "Apple IIe, ROM .*roms/ROM" "a machine without a ROM falls back to one with"

if [ -n "${BMAPPLE2_ROMS:-}" ]; then
    echo "Apple IIgs (real ROM from $BMAPPLE2_ROMS)"
    run iigs 60 "$BMAPPLE2_ROMS" "" '300 shot boot.ppm\n310 type PRINT "HELLO"\\n\n500 shot hello.ppm\n600 audio\n601 quit\n'
    check_log iigs "Read: (131072|262144) bytes of ROM" "IIgs ROM found"
    check_log iigs "machine 59\.9[0-9] Hz, synchronised to the display" "IIgs locked to 60 Hz"
    differ iigs boot hello
    no_underruns iigs

    echo "Disk activity (a IIe, its ROM made from MAME's)"
    # Booting a blank 5.25" disk: the light and S6 D1 T00 at the bottom
    # left. With the drive empty the IIe's boot ROM spins it for ever: "no
    # disk", in red. The ROMs are linked one by one, so that the IIe ROM
    # BMApple2 makes goes in the test's folder, not the ROM folder.
    for name in disk_iie disk_empty; do
        mkdir -p "$OUT/$name/roms"
        for f in "$BMAPPLE2_ROMS"/*; do ln -s "$f" "$OUT/$name/roms/" 2>/dev/null; done
    done
    python3 -c "open('$OUT/disk_iie/blank.dsk','wb').truncate(143360)"
    (cd "$OUT/disk_iie" && printf "120 run $OUT/disk_iie/blank.dsk\n140 shot disk.ppm\n141 quit\n" | \
        BMAPPLE2_REFRESH=60 BMAPPLE2_CMDLINE="root=. machine=iie" "$SIM" > log.txt 2>&1)
    (cd "$OUT/disk_empty" && printf '100 shot disk.ppm\n101 quit\n' | \
        BMAPPLE2_REFRESH=60 BMAPPLE2_CMDLINE="root=. machine=iie" "$SIM" > log.txt 2>&1)
    if grep -q "Apple IIe, ROM" "$OUT/disk_iie/log.txt"; then
        disk_colours() {
            python3 - "$1" <<'PYEOF'
import sys
data = open(sys.argv[1], 'rb').read()
parts = data.split(b'\n', 3)
w, h = map(int, parts[1].split())
px = parts[3]
red = green = 0
for y in range(h - 40, h):
    for x in range(0, 420):
        r, g, b = px[(y * w + x) * 3:(y * w + x) * 3 + 3]
        if r > 200 and g < 60 and b < 60: red += 1
        if g > 200 and r < 60 and b < 60: green += 1
print(red, green)
PYEOF
        }
        read red green <<< "$(disk_colours "$OUT/disk_iie/disk.ppm")"
        if [ "$red" -gt 0 ] && [ "$red" -lt 600 ] && [ "$green" -gt 200 ]; then
            pass "disk_iie: the light and S6 D1 T00 while booting a disk"
        else
            fail disk_iie "no disk activity shown (red $red, green $green)"
        fi
        read red green <<< "$(disk_colours "$OUT/disk_empty/disk.ppm")"
        if [ "$red" -gt 1000 ] && [ "$green" -eq 0 ]; then
            pass "disk_empty: an empty drive shown in red"
        else
            fail disk_empty "the empty drive not shown (red $red, green $green)"
        fi
    else
        echo "SKIP  disk activity: no IIe ROM (or MAME's parts for one) in BMAPPLE2_ROMS"
    fi

    echo "The virtual modem (slot 2)"
    # BASIC dials through the modem port's SCC data register ($C03A), as a
    # terminal program does, collects what comes back (the modem's CONNECT
    # and the server's WELCOME) and sends it to the server, which checks
    # it. The connection is made 120 frames late (in the background), and
    # every frame must still be shown. Then the same to a port nothing
    # listens on: NO CARRIER, and nothing stops.
    modem_basic() {
        printf '%s\n' "300 type 10 A\$=\"ATDT127.0.0.1:$1\":GOSUB 100\\n" \
            '800 type 20 B$="":FOR J=1 TO 2000:IF PEEK(49208)/2<>INT(PEEK(49208)/2) THEN B$=B$+CHR$(PEEK(49210))\n' \
            '1700 type 30 NEXT:A$=B$:GOSUB 100:END\n' \
            '2100 type 100 FOR I=1 TO LEN(A$):POKE 49210,ASC(MID$(A$,I,1)):NEXT:POKE 49210,13:RETURN\n' \
            '2900 type RUN\n' '4400 audio' '4401 quit'
    }
    mkdir -p "$OUT/modem/roms" "$OUT/modem_nc/roms"
    for f in "$BMAPPLE2_ROMS"/*; do
        ln -s "$f" "$OUT/modem/roms/" 2>/dev/null
        ln -s "$f" "$OUT/modem_nc/roms/" 2>/dev/null
    done
    python3 - "$OUT/modem/server.log" <<'EOF' &
import socket, sys
log = open(sys.argv[1], 'w')
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', 65514))
s.listen(1)
s.settimeout(60)
c, _ = s.accept()
log.write('connected\n')
log.flush()
c.sendall(b'WELCOME\r\n')
c.settimeout(60)
got = b''
while b'WELCOME' not in got:
    d = c.recv(256)
    if not d:
        break
    got += d
log.write('got back %r\n' % got)
log.flush()
EOF
    server=$!
    sleep 1
    (cd "$OUT/modem" && modem_basic 65514 | BMAPPLE2_SIM_CONNECT_FRAMES=120 \
        BMAPPLE2_IP=127.0.0.1 BMAPPLE2_CMDLINE="root=." "$SIM" > log.txt 2>&1)
    kill $server 2>/dev/null
    wait $server 2>/dev/null
    check_log modem "SCC port 1 is now outgoing to 127.0.0.1:65514" "ATDT connects (in the background)"
    if grep -q "got back .*CONNECT 2400.*WELCOME" "$OUT/modem/server.log" 2>/dev/null; then
        pass "modem: the Apple got CONNECT 2400 and the server's WELCOME, and sent them back"
    else
        fail modem "the Apple didn't get CONNECT and WELCOME (see out/modem/server.log)"
    fi
    (cd "$OUT/modem_nc" && modem_basic 65515 | BMAPPLE2_SIM_CONNECT_FRAMES=120 \
        BMAPPLE2_IP=127.0.0.1 BMAPPLE2_CMDLINE="root=." "$SIM" > log.txt 2>&1)
    check_log modem_nc "can't connect to 127.0.0.1:65515" "a call nobody answers fails"
    for name in modem modem_nc; do
        line=$(grep "frames presented" "$OUT/$name/log.txt" | tail -1)
        presented=$(echo "$line" | sed 's/.*: \([0-9]*\) frames presented/\1/')
        if [ "${presented:-0}" -ge 4400 ]; then
            pass "$name: the Apple kept going while dialling ($presented/4400 frames)"
        else
            fail $name "frames presented: $line"
        fi
    done

    echo "Apple IIgs: switching machines"
    # The IIgs's ROM folder with the stand-in IIe ROM added (as APPLE2E.ROM).
    mkdir -p "$OUT/bothroms"
    for f in "$BMAPPLE2_ROMS"/*; do ln -s "$f" "$OUT/bothroms/" 2>/dev/null; done
    # (rm first: it is a link to the ROM folder's own, which cp would
    # overwrite)
    rm -f "$OUT/bothroms/APPLE2E.ROM"
    cp "$TESTROMS/ROM" "$OUT/bothroms/APPLE2E.ROM"
    # Menu, Machine (three down), the IIe (last), and its saved choice.
    run switch 60 "$OUT/bothroms" "" "200 key $F10 1\n201 key $F10 0\n210 key $DOWN 1\n211 key $DOWN 0\n212 key $DOWN 1\n213 key $DOWN 0\n214 key $DOWN 1\n215 key $DOWN 0\n220 key $ENTER 1\n221 key $ENTER 0\n230 key $DOWN 1\n231 key $DOWN 0\n232 key $DOWN 1\n233 key $DOWN 0\n240 key $ENTER 1\n241 key $ENTER 0\n400 shot iie.ppm\n401 quit\n"
    check_log switch "machine Apple IIe, ROM .*APPLE2E\.ROM" "the menu switches to the IIe"
    check_log switch "Read: 32768 bytes of ROM" "KEGS loads the IIe's ROM"
    if grep -q "^machine=iie" "$OUT/switch/bmapple2.cfg" 2>/dev/null; then
        pass "switch: machine=iie saved to bmapple2.cfg"
    else
        fail switch "machine=iie not in bmapple2.cfg"
    fi

    echo "Apple IIgs starts a 5.25\" disk from the web UI"
    # A boot sector that writes BOOTED at the top left of the text screen.
    python3 - "$OUT/disks/boot.dsk" <<'EOF'
import sys
prog = []
for i, c in enumerate("BOOTED"):
    prog += [0xA9, ord(c) | 0x80, 0x8D, (0x400 + i) & 0xff, (0x400 + i) >> 8]
loop = 0x801 + len(prog)
prog += [0x4C, loop & 0xff, loop >> 8]
img = bytearray(143360)
img[:1 + len(prog)] = bytes([1] + prog)
open(sys.argv[1], 'wb').write(img)
EOF
    run iigs525 60 "$BMAPPLE2_ROMS" "" "120 run $OUT/disks/boot.dsk\n900 shot booted.ppm\n901 quit\n"
    check_log iigs525 "startup slot set back to" "the startup slot is put back"
    # BOOTED is white letters in text row 0; without it the row is blank.
    if python3 - "$OUT/iigs525/booted.ppm" <<'EOF'
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
w = int(fields[1])
px = data[pos + 1:]
white = sum(1 for y in range(60, 100) for x in range(30, 120)
            if px[(y * w + x) * 3:(y * w + x) * 3 + 3] == b'\xff\xff\xff')
sys.exit(0 if white > 50 else 1)
EOF
    then
        pass "iigs525: the disk started (BOOTED on the screen)"
    else
        fail iigs525 "the disk didn't start (see out/iigs525/booted.ppm)"
    fi
fi

echo
if [ $FAILED -eq 0 ]; then
    echo "All tests passed."
else
    echo "$FAILED test(s) failed."
fi
exit $((FAILED > 0))
