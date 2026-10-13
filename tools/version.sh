#!/bin/sh
#
# Prints the BMApple2 version: the semantic version from the VERSION file
# and the git commit, for example "1.0.0 (288b2f4)".
#   --semver  only the semantic version
#   --git     only the git commit, with "-dirty" if BMApple2's own files
#             have uncommitted changes. Changes inside the submodules
#             (make_all.sh patches Circle) don't count.
#
cd "$(dirname "$0")/.." || exit 1
semver=$(cat VERSION 2>/dev/null) || semver=dev
if git=$(git rev-parse --short HEAD 2>/dev/null); then
    if ! git diff --quiet --ignore-submodules HEAD 2>/dev/null; then
        git="$git-dirty"
    fi
else
    git=unknown
fi
case "$1" in
--semver) echo "$semver" ;;
--git) echo "$git" ;;
*) echo "$semver ($git)" ;;
esac
