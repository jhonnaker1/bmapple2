#!/bin/bash
#
# Cleans everything: the kernel, the compiled XRoar library, the SD card
# staging folder, and the third party builds inside circle-stdlib (which is
# also reset to its pinned revision, undoing src/patches/).
#
#   ./clean_all.sh [--keep-sdcard]
#
# --keep-sdcard keeps build/sdcard, so build_sdcard.sh can clean between
# boards while it fills the card.
#

if [ ! -f sdcard/config.txt ]
then
    echo "Must be run from the BMCoco root directory." >&2
    exit 1
fi

KEEP_SDCARD=0
case "${1:-}" in
    "") ;;
    --keep-sdcard) KEEP_SDCARD=1 ;;
    *) echo "Usage: $0 [--keep-sdcard]" >&2; exit 1 ;;
esac

echo "Cleaning..."
SRC_DIR=$(pwd)

if [ -f third_party/circle-stdlib/Config.mk ]
then
    make clean >/dev/null 2>&1
fi
if [ "$KEEP_SDCARD" -eq 1 ]
then
    find build -mindepth 1 -maxdepth 1 ! -name sdcard -exec rm -rf {} + 2>/dev/null
else
    rm -rf build
fi
rm -f kernel*.img kernel*.elf kernel*.lst kernel*.map

cd "$SRC_DIR/third_party/circle-stdlib" || exit 1
find . -name config.cache -exec rm -f {} \;
make clean >/dev/null 2>&1
make mrproper >/dev/null 2>&1
rm -rf build install
git reset -q --hard
git clean -q -fdx

for lib in libs/circle libs/circle-newlib
do
    (cd "$lib" && git reset -q --hard && git clean -q -fdx)
done
(cd libs/circle/addon/wlan/hostap 2>/dev/null && git reset -q --hard && git clean -q -fdx)

echo "Clean."
