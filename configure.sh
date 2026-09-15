#!/bin/bash
# Configure CMake build directory.
#
# Usage:
#   ./cmake-conf.sh          # Debug build in build/
#   ./cmake-conf.sh -r       # Release build in build-release/
#
# To use command line tools instead of XCode:
# sudo xcode-select -s /Library/Developer/CommandLineTools

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
BREW_PREFIX=$(brew --prefix 2>/dev/null)
if [ -z "$BREW_PREFIX" ]; then
    echo "Homebrew is required to configure Linea on macOS." >&2
    exit 1
fi
BUILD_TYPE=Debug
BUILD_DIR="$ROOT_DIR/build"

while getopts "r" opt; do
    case $opt in
        r) BUILD_TYPE=Release; BUILD_DIR="$ROOT_DIR/build-release" ;;
        *) echo "Usage: $0 [-r]" >&2; exit 1 ;;
    esac
done

# Keep the compiler SDK aligned with Homebrew's pkg-config files.  Those files
# may contain absolute SDK include paths, so selecting a different SDK through
# xcrun produces a mixed libc++/system-header build after an OS upgrade.
BREW_ENV=$(brew --env 2>/dev/null)
BREW_SDKROOT=$(printf '%s\n' "$BREW_ENV" | sed -n 's/^export HOMEBREW_SDKROOT="\(.*\)"$/\1/p')
CURRENT_SDK=$(xcrun --show-sdk-path 2>/dev/null)
SDKROOT="$BREW_SDKROOT"
if [ -z "$SDKROOT" ] || [ ! -d "$SDKROOT" ]; then
    SDKROOT="$CURRENT_SDK"
fi
if [ -n "$SDKROOT" ] && [ -d "$SDKROOT" ]; then
    SDKROOT=$(cd "$SDKROOT" && pwd -P)
    SDK_VERSION=$(basename "$SDKROOT" | sed -n 's/^MacOSX\([0-9][0-9]*\)\(\.[0-9][0-9]*\)*\.sdk$/\1/p')
    if [ -z "$SDK_VERSION" ]; then
        SDK_VERSION=$(xcrun --show-sdk-version 2>/dev/null | cut -d. -f1)
    fi
    export HOMEBREW_SDKROOT="$SDKROOT"
    if [ -n "$SDK_VERSION" ]; then
        export PKG_CONFIG_LIBDIR="$BREW_PREFIX/lib/pkgconfig:$BREW_PREFIX/share/pkgconfig:/usr/lib/pkgconfig:$BREW_PREFIX/Library/Homebrew/os/mac/pkgconfig/${SDK_VERSION}"
    fi
    CMAKE_OSX_SYSROOT="$SDKROOT"
fi

# icu4c is keg-only, so its .pc files are not on the default pkg-config
# search path.  Prefer the unversioned formula, else the newest icu4c@* keg.
ICU_PREFIX=$(brew --prefix icu4c 2>/dev/null)
if [ -z "$ICU_PREFIX" ]; then
    ICU_FORMULA=$(brew list --formula | awk '/^icu4c@/ {print}' | sort -rV | head -1)
    if [ -n "$ICU_FORMULA" ]; then
        ICU_PREFIX=$(brew --prefix "$ICU_FORMULA" 2>/dev/null)
    fi
fi
if [ -n "$ICU_PREFIX" ] && [ -d "$ICU_PREFIX/lib/pkgconfig" ]; then
    export PKG_CONFIG_PATH="$ICU_PREFIX/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

cmake --fresh -S "$ROOT_DIR" \
    -B "$BUILD_DIR" \
    -G Ninja \
    ${CMAKE_OSX_SYSROOT:+-DCMAKE_OSX_SYSROOT="$CMAKE_OSX_SYSROOT"} \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install"
