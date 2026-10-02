#!/bin/bash
# Generate linea.icns for the macOS app bundle.
#
# Sources:
#   linea-16.svg — artwork designed for small sizes (rendered below 64px)
#   linea-64.svg — artwork for 64px and up
#
# Requires a Linea binary (for its headless -o/-w/--export-height export)
# and iconutil (in macOS). Run from any directory; output lands next to
# the sources.

set -euo pipefail

cd "$(dirname "$0")"

LINEA="${LINEA:-$(dirname "$0")/../../build/bin/linea.app/Contents/MacOS/linea}"
SRC_SMALL="linea-16.svg"
SRC_LARGE="linea-64.svg"
ICONSET="linea.iconset"
OUT="linea.icns"

rm -rf "$ICONSET"
mkdir -p "$ICONSET"

# render <pixel size> <iconset filename>
render() {
    local px="$1" name="$2"
    local src="$SRC_LARGE"
    if [ "$px" -lt 64 ]; then
        src="$SRC_SMALL"
    fi
    "$LINEA" "$src" -o "$ICONSET/$name" -w "$px" --export-height "$px"
}

render 16   icon_16x16.png
render 32   icon_16x16@2x.png
render 32   icon_32x32.png
render 64   icon_32x32@2x.png
render 128  icon_128x128.png
render 256  icon_128x128@2x.png
render 256  icon_256x256.png
render 512  icon_256x256@2x.png
render 512  icon_512x512.png
render 1024 icon_512x512@2x.png

iconutil -c icns "$ICONSET" -o "$OUT"
rm -rf "$ICONSET"

echo "Wrote $OUT"
