#!/bin/bash
#
# Runs BMCoco in the host simulator in real time, shown in a web page:
#
#   tools/hostsim/live.sh [ROM_DIRECTORY] [PORT]
#
# then open http://127.0.0.1:PORT/ (default port 8064). ROM_DIRECTORY holds
# your CoCo ROMs (bas13.rom, extbas11.rom, disk11.rom, coco3.rom); without
# it the stand-in test ROM is used. The simulator's SD card is
# tools/hostsim/out/live: put disks, tapes and programs there. Choosing
# Reboot (or Shut Down) in the menu starts it again. Ctrl+C stops it.
#
set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROMS=${1:-}
PORT=${2:-8064}
CARD=$HERE/out/live

make -s -C "$HERE" -j4 || exit 1

mkdir -p "$CARD"/{disks,tapes,carts,programs}
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
    BMCOCO_LIVE=$PORT BMCOCO_CMDLINE="root=$CARD ${BMCOCO_CMDLINE:-}" \
        "$HERE/build/bmcoco_sim" < /dev/null
    status=$?
    [ $status -ne 0 ] && exit $status
    echo "[live] restarting"
    sleep 1
done
