#!/bin/bash
#
# Tests the Pi 4's software smooth scaling and CRT shader (src/pi4/scaler.*)
# and how a frame is shared between cores (src/pi4/frameshare.h) on the PC.
# The pictures go to tools/pi4scaler/out/.
#
#   tools/pi4scaler/run_tests.sh
#

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
make -s -C "$HERE"
rm -rf "$HERE/out"
mkdir -p "$HERE/out"
"$HERE/build/pi4scaler_test" "$HERE/out"
echo
"$HERE/build/share_test"
