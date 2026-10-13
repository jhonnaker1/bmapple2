#!/bin/bash
#
# Runs BMApple2 in the host simulator in real time, shown in a web page:
#
#   tools/hostsim/live.sh [ROM_DIRECTORY] [PORT]
#
# then open http://127.0.0.1:PORT/ (default port 8064). ROM_DIRECTORY holds
# your Apple IIgs or IIe ROM (ROM.03, ROM.01, APPLE2E.ROM, or MAME's chips;
# see docs/ROMS.txt); without it the stand-in test ROM is used. The
# simulator's SD card is tools/hostsim/out/live: put disk images in its
# disks folder and mount them with F4. Choosing Reboot (or Shut Down) in the
# menu starts it again. Ctrl+C stops it.
#
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROMS=${1:-}
PORT=${2:-8064}
CARD=$HERE/out/live

make -s -C "$HERE" -j4 || exit 1

mkdir -p "$CARD"/disks
rm -f "$CARD/roms"
if [ -n "$ROMS" ]; then
    ln -s "$ROMS" "$CARD/roms"
else
    mkdir -p "$CARD/testroms"
    python3 "$HERE/make_test_rom.py" "$CARD/testroms" > /dev/null
    ln -s "$CARD/testroms" "$CARD/roms"
fi

cd "$CARD" || exit 1
while true; do
    BMAPPLE2_LIVE=$PORT BMAPPLE2_CMDLINE="root=$CARD ${BMAPPLE2_CMDLINE:-}" \
        "$HERE/build/bmapple2_sim" < /dev/null
    status=$?
    [ $status -ne 0 ] && exit $status
    echo "[live] restarting"
    sleep 1
done
