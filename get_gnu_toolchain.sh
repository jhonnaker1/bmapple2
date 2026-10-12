#!/bin/bash
#
# Source this file to put the Arm GNU Toolchain used by BMCoco on PATH.
#
#   source ./get_gnu_toolchain.sh
#
# The pinned release is downloaded into ~/.cache/bmcoco/toolchains the
# first time. A matching toolchain already on PATH, or in $ARM_HOME, is used
# as it is. Set BMCOCO_ALLOW_ANY_TOOLCHAIN=1 to accept whatever
# arm-none-eabi-gcc is on PATH (for example the distribution's package) when
# the pinned release cannot be downloaded.

TOOLCHAIN_VERSION=15.2.1
TOOLCHAIN_RELEASE=15.2.rel1
# The build for this computer: Linux (x86_64 or aarch64) or macOS.
case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) TOOLCHAIN_HOST=darwin-arm64 ;;
    Darwin-x86_64) TOOLCHAIN_HOST=darwin-x86_64 ;;
    Linux-aarch64|Linux-arm64) TOOLCHAIN_HOST=aarch64 ;;
    *) TOOLCHAIN_HOST=x86_64 ;;
esac
TOOLCHAIN_NAME=arm-gnu-toolchain-${TOOLCHAIN_RELEASE}-${TOOLCHAIN_HOST}-arm-none-eabi
TOOLCHAIN_ARCHIVE=${TOOLCHAIN_NAME}.tar.xz
TOOLCHAIN_URL=https://gitlab.arm.com/api/v4/projects/tooling%2Fgnu-toolchains-for-arm/packages/generic/gnu-toolchain/${TOOLCHAIN_RELEASE}/${TOOLCHAIN_ARCHIVE}
TOOLCHAIN_CACHE=${XDG_CACHE_HOME:-$HOME/.cache}/bmcoco/toolchains
TOOLCHAIN_HOME=${BMCOCO_TOOLCHAIN_HOME:-$TOOLCHAIN_CACHE/$TOOLCHAIN_NAME}

toolchain_is_valid()
{
    [ -x "$1/bin/arm-none-eabi-gcc" ] &&
        [ "$("$1/bin/arm-none-eabi-gcc" -dumpfullversion 2>/dev/null)" = "$TOOLCHAIN_VERSION" ]
}

toolchain_use()
{
    export ARM_HOME=$1
    export ARM_VERSION=$("$1/bin/arm-none-eabi-gcc" -dumpfullversion)
    case ":$PATH:" in
        *":$ARM_HOME/bin:"*) ;;
        *) export PATH=$ARM_HOME/bin:$PATH ;;
    esac
    echo "Using Arm GNU Toolchain $ARM_VERSION: $ARM_HOME"
}

toolchain_download()
{
    mkdir -p "$TOOLCHAIN_CACHE" || return 1
    local archive_path=$TOOLCHAIN_CACHE/$TOOLCHAIN_ARCHIVE

    if ! tar -tJf "$archive_path" >/dev/null 2>&1
    then
        rm -f "$archive_path"
        echo "Downloading Arm GNU Toolchain $TOOLCHAIN_RELEASE..."
        if command -v curl >/dev/null 2>&1
        then
            curl --fail --location --retry 3 --output "$archive_path" "$TOOLCHAIN_URL" || return 1
        elif command -v wget >/dev/null 2>&1
        then
            wget --output-document="$archive_path" "$TOOLCHAIN_URL" || return 1
        else
            echo "curl or wget is required to download the toolchain." >&2
            return 1
        fi
    fi

    local install_path=$TOOLCHAIN_CACHE/.${TOOLCHAIN_NAME}.installing
    rm -rf "$install_path"
    mkdir -p "$install_path" || return 1
    echo "Installing Arm GNU Toolchain $TOOLCHAIN_RELEASE..."
    tar -xJf "$archive_path" -C "$install_path" || return 1
    rm -rf "$TOOLCHAIN_HOME"
    mv "$install_path/$TOOLCHAIN_NAME" "$TOOLCHAIN_HOME" || return 1
    rmdir "$install_path"
}

if toolchain_is_valid "${ARM_HOME:-}"
then
    TOOLCHAIN_HOME=$ARM_HOME
elif command -v arm-none-eabi-gcc >/dev/null 2>&1
then
    path_toolchain=$(dirname "$(dirname "$(readlink -f "$(command -v arm-none-eabi-gcc)")")")
    if toolchain_is_valid "$path_toolchain"
    then
        TOOLCHAIN_HOME=$path_toolchain
    fi
fi

if ! toolchain_is_valid "$TOOLCHAIN_HOME"
then
    toolchain_download || true
fi

if toolchain_is_valid "$TOOLCHAIN_HOME"
then
    toolchain_use "$TOOLCHAIN_HOME"
elif [ "${BMCOCO_ALLOW_ANY_TOOLCHAIN:-0}" = "1" ] && command -v arm-none-eabi-gcc >/dev/null 2>&1
then
    echo "WARNING: Arm GNU Toolchain $TOOLCHAIN_VERSION is not available." >&2
    toolchain_use "$(dirname "$(dirname "$(readlink -f "$(command -v arm-none-eabi-gcc)")")")"
else
    echo "Arm GNU Toolchain $TOOLCHAIN_VERSION could not be installed." >&2
    echo "Set BMCOCO_ALLOW_ANY_TOOLCHAIN=1 to build with the arm-none-eabi-gcc on PATH." >&2
    return 1 2>/dev/null || exit 1
fi
