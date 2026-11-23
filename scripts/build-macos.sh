#!/bin/bash
# VitaPlug LAB-D · compilación Release universal en macOS (Command Line Tools o Xcode).
#   scripts/build-macos.sh [directorio_build]
# 1. Obtiene JUCE 9.0.3 dentro del proyecto si falta y comprueba la revisión.
# 2. Configura y compila (arm64 + x86_64, mínimo declarado 10.15; el enlazador
#    eleva el segmento arm64 a 11.0).
# 3. Ejecuta CTest.
# 4. Firma ad hoc los bundles para la prueba local.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${1:-$root/build/release}"
juce_rev="be29c81492b6151c8ea8d14c840e1311963b3a83"

if [ ! -f "$root/external/JUCE/CMakeLists.txt" ]; then
    git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git "$root/external/JUCE"
fi
actual="$(git -C "$root/external/JUCE" rev-parse HEAD)"
if [ "$actual" != "$juce_rev" ]; then
    echo "JUCE en $actual; se esperaba $juce_rev (9.0.3)" >&2
    exit 1
fi

cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" -j"$(sysctl -n hw.ncpu)"
ctest --test-dir "$build" --output-on-failure

for bundle in "$build/LABD_artefacts/Release/AU/LAB-D.component" "$build/LABD_artefacts/Release/VST3/LAB-D.vst3"; do
    codesign --force --sign - --timestamp=none "$bundle"
    codesign --verify --strict --verbose=1 "$bundle"
done
echo "Compilado en $build/LABD_artefacts/Release"
