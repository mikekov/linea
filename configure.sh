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

# Resolve the current macOS SDK.  Homebrew's brew --env caches the SDK that
# was active when Homebrew was installed; if the CLT has been updated since,
# the cached paths point at a stale SDK and pollute the include search path
# (mixing e.g. MacOSX26 math.h with MacOSX27 libc++).  Override the stale
# Homebrew values so pkg-config and CMake use the current SDK consistently.
CURRENT_SDK=$(xcrun --show-sdk-path 2>/dev/null)
CURRENT_SDK_VER=$(xcrun --show-sdk-version 2>/dev/null | cut -d. -f1)
if [ -n "$CURRENT_SDK" ] && [ -n "$CURRENT_SDK_VER" ]; then
    export HOMEBREW_SDKROOT="$CURRENT_SDK"
    export PKG_CONFIG_LIBDIR="$BREW_PREFIX/lib/pkgconfig:$BREW_PREFIX/share/pkgconfig:/usr/lib/pkgconfig:$BREW_PREFIX/Library/Homebrew/os/mac/pkgconfig/${CURRENT_SDK_VER}"
    export CMAKE_INCLUDE_PATH="${CURRENT_SDK}/System/Library/Frameworks/OpenGL.framework/Versions/Current/Headers"
    export CMAKE_LIBRARY_PATH="${CURRENT_SDK}/System/Library/Frameworks/OpenGL.framework/Versions/Current/Libraries"
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
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install"
