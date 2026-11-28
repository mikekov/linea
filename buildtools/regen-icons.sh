#!/usr/bin/env bash
# Regenerate themed icon resources without a CMake reconfigure.
# Mirrors the add_custom_command chain in src/CMakeLists.txt so newly added
# SVGs in share/icons/Dash get picked up by the file(GLOB).
#
# Usage: buildtools/regen-icons.sh [build-dir]   (default: ./build)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$(cd "${1:-$ROOT/build}" && pwd)"

python3 "$ROOT/buildtools/process-icons.py" \
    --source "$ROOT/share/icons/Dash/symbolic/actions" \
             "$ROOT/share/icons/Dash/scalable/actions" \
    --build-dir "$BUILD_DIR"

RCC="${RCC:-$(command -v rcc || true)}"
if [ -z "$RCC" ]; then
    RCC="$(brew --prefix qtbase 2>/dev/null)/share/qt/libexec/rcc"
fi

"$RCC" -binary "$BUILD_DIR/icons-light.qrc" -o "$BUILD_DIR/icons-light.rcc"
"$RCC" -binary "$BUILD_DIR/icons-dark.qrc"  -o "$BUILD_DIR/icons-dark.rcc"

cat > "$BUILD_DIR/embedded-icons.qrc" <<'QRC'
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/">
        <file alias="icons-light.rcc">icons-light.rcc</file>
        <file alias="icons-dark.rcc">icons-dark.rcc</file>
    </qresource>
</RCC>
QRC

(cd "$BUILD_DIR" && "$RCC" embedded-icons.qrc -o embedded-icons.cpp)

echo "Icons regenerated in $BUILD_DIR — rebuild with: ninja -C $BUILD_DIR linea_base"
