#!/bin/bash
# VitaPlug LAB-D · paquetes para smoke en macOS.
#   scripts/package-macos.sh [build] [dist]
# Crea un ZIP por formato que conserva el bundle (ditto --keepParent), calcula
# SHA-256, extrae cada ZIP en una carpeta temporal y comprueba que el binario
# extraído es idéntico al compilado y validado.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$root/build/release}"
version="0.1.0"
rev="$(git -C "$root" rev-parse --short HEAD)"
dist="${2:-$root/dist/LAB-D-$version-macOS-universal-$rev}"
art="$build/LABD_artefacts/Release"
mkdir -p "$dist"

zip_au="$dist/LAB-D-$version-macOS-universal-AU.zip"
zip_vst3="$dist/LAB-D-$version-macOS-universal-VST3.zip"
rm -f "$zip_au" "$zip_vst3"
ditto -c -k --sequesterRsrc --keepParent "$art/AU/LAB-D.component" "$zip_au"
ditto -c -k --sequesterRsrc --keepParent "$art/VST3/LAB-D.vst3" "$zip_vst3"

check="$(mktemp -d)"
trap 'rm -rf "$check"' EXIT
ditto -x -k "$zip_au" "$check/au"
ditto -x -k "$zip_vst3" "$check/vst3"
built_au="$(shasum -a 256 "$art/AU/LAB-D.component/Contents/MacOS/LAB-D" | cut -d' ' -f1)"
built_vst3="$(shasum -a 256 "$art/VST3/LAB-D.vst3/Contents/MacOS/LAB-D" | cut -d' ' -f1)"
x_au="$(shasum -a 256 "$check/au/LAB-D.component/Contents/MacOS/LAB-D" | cut -d' ' -f1)"
x_vst3="$(shasum -a 256 "$check/vst3/LAB-D.vst3/Contents/MacOS/LAB-D" | cut -d' ' -f1)"
[ "$built_au" = "$x_au" ] || { echo "AU extraído distinto del compilado" >&2; exit 1; }
[ "$built_vst3" = "$x_vst3" ] || { echo "VST3 extraído distinto del compilado" >&2; exit 1; }
codesign --verify --strict "$check/au/LAB-D.component"
codesign --verify --strict "$check/vst3/LAB-D.vst3"

{
    echo "# VitaPlug LAB-D $version · macOS universal (arm64 + x86_64) · revisión $rev"
    echo "# ZIP"
    (cd "$dist" && shasum -a 256 "$(basename "$zip_au")" "$(basename "$zip_vst3")")
    echo "# binarios dentro de los bundles (idénticos al compilado y al extraído)"
    echo "$built_au  LAB-D.component/Contents/MacOS/LAB-D"
    echo "$built_vst3  LAB-D.vst3/Contents/MacOS/LAB-D"
} > "$dist/SHA256SUMS.txt"
cat "$dist/SHA256SUMS.txt"
echo "Paquetes en $dist"
