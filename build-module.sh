#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
REPO_URL=https://github.com/supremegamers/vendor_intel_proprietary_houdini.git
COMMIT=2f8f088671182e17e67321e098e8411a3972a628
CACHE_ROOT="${XDG_CACHE_HOME:-$HOME/.cache}/tft-waydroid"
SOURCE="$CACHE_ROOT/hpe14-runtime"
OUTPUT="${1:-$ROOT/tft-waydroid-hpe14.zip}"

for tool in git python3 gcc; do
  command -v "$tool" >/dev/null || { echo "Missing required tool: $tool" >&2; exit 1; }
done
mkdir -p "$CACHE_ROOT"
if [ ! -d "$SOURCE/.git" ]; then
  git init "$SOURCE"
fi
git -C "$SOURCE" remote set-url origin "$REPO_URL" 2>/dev/null || \
  git -C "$SOURCE" remote add origin "$REPO_URL"
git -C "$SOURCE" fetch --depth 1 origin "$COMMIT"
git -C "$SOURCE" checkout --detach FETCH_HEAD
python3 "$ROOT/build-module.py" "$SOURCE/prebuilts" --output "$OUTPUT"
