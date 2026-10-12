#!/bin/sh
#
# Prints the version of XRoar in third_party/xroar, from its configure.ac
# (for example "1.12.1").
#
cd "$(dirname "$0")/.." || exit 1
sed -n 's/^AC_INIT(\[xroar\],\[\([^]]*\)\].*/\1/p' third_party/xroar/configure.ac
