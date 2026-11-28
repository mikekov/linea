#!/usr/bin/env bash
# Sync the canonical 7-column "mainGrid" layout across the panel .ui files.
# Thin wrapper so the sync can be run like the other buildtools scripts.
#
# Usage: buildtools/sync-grid-layouts.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

python3 "$ROOT/buildtools/sync-grid-layouts.py"
