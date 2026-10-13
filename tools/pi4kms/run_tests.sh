#!/bin/bash
#
# Tests the parts of the Pi 4's direct display driver (src/pi4kms/) that
# are pure calculation, on the PC.
#
#   tools/pi4kms/run_tests.sh
#

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
make -s -C "$HERE"
"$HERE/build/pi4kms_test"
