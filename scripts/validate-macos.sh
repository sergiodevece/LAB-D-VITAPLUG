#!/bin/bash
# VitaPlug LAB-D · validación de formatos en macOS.
#   LABD_PLUGINVAL=/ruta/pluginval.app/Contents/MacOS/pluginval scripts/validate-macos.sh [build] [evidencias]
#
# - Comprueba arquitecturas y mínimos reales de los ejecutables (lipo, vtool).
# - Instala una copia del AU en ~/Library/Audio/Plug-Ins/Components (auval solo
#   valida componentes registrados) y ejecuta auval en arm64 y en x86_64 (Rosetta).
# - Valida el VST3 (y el AU) con pluginval si se indica LABD_PLUGINVAL.
# Las salidas completas quedan en la carpeta de evidencias.
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$root/build/release}"
evidence="${2:-$root/docs/evidence}"
mkdir -p "$evidence"
art="$build/LABD_artefacts/Release"
au="$art/AU/LAB-D.component"
vst3="$art/VST3/LAB-D.vst3"
status=0

{
    echo "== lipo =="
    lipo -info "$au/Contents/MacOS/LAB-D" "$vst3/Contents/MacOS/LAB-D"
    for slice in arm64 x86_64; do
        echo "== vtool $slice (AU) =="
        vtool -arch "$slice" -show-build "$au/Contents/MacOS/LAB-D" | grep -E "platform|minos|sdk"
        echo "== vtool $slice (VST3) =="
        vtool -arch "$slice" -show-build "$vst3/Contents/MacOS/LAB-D" | grep -E "platform|minos|sdk"
    done
    echo "== codesign =="
    codesign -dv "$au" 2>&1 | grep -E "Identifier|Signature|flags|TeamIdentifier"
    codesign -dv "$vst3" 2>&1 | grep -E "Identifier|Signature|flags|TeamIdentifier"
} > "$evidence/binarios.txt" 2>&1
cat "$evidence/binarios.txt"

# AU: copia en la carpeta del usuario para que el sistema la registre.
dest="$HOME/Library/Audio/Plug-Ins/Components/LAB-D.component"
rm -rf "$dest"
ditto "$au" "$dest"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2

auval -v aufx LbDy Vtpl > "$evidence/auval-arm64.txt" 2>&1
grep -q "AU VALIDATION SUCCEEDED" "$evidence/auval-arm64.txt" && echo "auval arm64: SUCCEEDED" || { echo "auval arm64: FAILED"; status=1; }
arch -x86_64 auval -v aufx LbDy Vtpl > "$evidence/auval-x86_64.txt" 2>&1
grep -q "AU VALIDATION SUCCEEDED" "$evidence/auval-x86_64.txt" && echo "auval x86_64 (Rosetta): SUCCEEDED" || { echo "auval x86_64: FAILED"; status=1; }

if [ -n "${LABD_PLUGINVAL:-}" ]; then
    "$LABD_PLUGINVAL" --version > "$evidence/pluginval-version.txt" 2>&1
    "$LABD_PLUGINVAL" --strictness-level 10 --random-seed 314159 --validate-in-process --output-dir "$evidence" \
        --validate "$vst3" > "$evidence/pluginval-vst3-arm64.txt" 2>&1
    grep -q "SUCCESS" "$evidence/pluginval-vst3-arm64.txt" && echo "pluginval VST3 arm64: SUCCESS" || { echo "pluginval VST3 arm64: FAILED"; status=1; }
    arch -x86_64 "$LABD_PLUGINVAL" --strictness-level 10 --random-seed 314159 --validate-in-process \
        --validate "$vst3" > "$evidence/pluginval-vst3-x86_64.txt" 2>&1
    grep -q "SUCCESS" "$evidence/pluginval-vst3-x86_64.txt" && echo "pluginval VST3 x86_64 (Rosetta): SUCCESS" || { echo "pluginval VST3 x86_64: FAILED"; status=1; }
    "$LABD_PLUGINVAL" --strictness-level 10 --random-seed 314159 --validate-in-process \
        --validate "$dest" > "$evidence/pluginval-au-arm64.txt" 2>&1
    grep -q "SUCCESS" "$evidence/pluginval-au-arm64.txt" && echo "pluginval AU arm64: SUCCESS" || { echo "pluginval AU arm64: FAILED"; status=1; }
else
    echo "pluginval: NO EJECUTADO (define LABD_PLUGINVAL)"
fi
exit $status
