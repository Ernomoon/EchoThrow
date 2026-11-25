#!/usr/bin/env bash
# Usage: build_pkg.sh <path/to/EchoThrow.vst3> <version>
set -euo pipefail
BUNDLE="$1"
VERSION="${2:-1.0.0}"
VERSION="${VERSION#v}"
# pkg versions must look like numbers; fall back for "dev-12"
[[ "$VERSION" =~ ^[0-9]+(\.[0-9]+)*$ ]] || VERSION="1.0.0"

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/Output"
ROOT="$(mktemp -d)"
rm -rf "$OUT"; mkdir -p "$OUT"
mkdir -p "$ROOT/Library/Audio/Plug-Ins/VST3"
cp -R "$BUNDLE" "$ROOT/Library/Audio/Plug-Ins/VST3/"

# ad-hoc signature so the bundle is at least internally consistent (Apple Silicon requires a signature)
codesign --force --deep --sign - "$ROOT/Library/Audio/Plug-Ins/VST3/EchoThrow.vst3"

pkgbuild --root "$ROOT" \
         --identifier com.echothrow.vst3 \
         --version "$VERSION" \
         --install-location / \
         "$OUT/EchoThrow-macOS.pkg"
echo "Built $OUT/EchoThrow-macOS.pkg"
