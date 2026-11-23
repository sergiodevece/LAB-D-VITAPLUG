# VitaPlug LAB-D 0.1.0

Delay estéreo de producción y mezcla con DSP propio, ECHO EQ de seis bandas a la salida
del bucle, LFO con siete destinos y el frontal aprobado por SerxDev («Designed by
SerxDev»). C++20 · JUCE 9.0.3 · CMake · Audio Unit v2 (`aufx`) y VST3.

- Encargo: [`docs/reference/ENCARGO.md`](docs/reference/ENCARGO.md) · frontal aprobado:
  [`docs/reference/LAB-D_Frontal.png`](docs/reference/LAB-D_Frontal.png)
- Motor y decisiones de diseño: [`docs/DSP.md`](docs/DSP.md)
- Parámetros, IDs e INIT: [`docs/PARAMETROS.md`](docs/PARAMETROS.md) · ajuste de la
  maqueta: [`docs/FICHA_MAQUETA.md`](docs/FICHA_MAQUETA.md)
- Pruebas y validación: [`docs/PRUEBAS.md`](docs/PRUEBAS.md) · autoauditoría:
  [`docs/AUDITORIA.md`](docs/AUDITORIA.md)
- Smoke en DAW: [`SMOKE.md`](SMOKE.md) · Windows: [`docs/WINDOWS.md`](docs/WINDOWS.md)

## Estructura

| Carpeta | Contenido |
|---|---|
| `src/engine/` | Motor de audio C++ sin JUCE ni DAW (reutilizable en AI DELAY) |
| `src/plugin/` | Wrapper JUCE: parámetros, procesador, estado, historial de undo |
| `src/ui/` | Editor sobre el frontal aprobado (`assets/LAB-D_Frontal.png`) |
| `tests/engine/`, `tests/plugin/` | Pruebas del motor (sin JUCE) y del plugin integrado, en CTest |
| `tools/` | Bancos de CPU y captura del editor real fuera de pantalla |
| `scripts/` | Compilación, validación y paquetes en macOS |
| `docs/evidence/` | Salidas de pruebas, auval, pluginval, CPU y capturas |

## Requisitos

- macOS 11 o posterior para compilar (probado en macOS 26.5, Apple M5).
- Apple Command Line Tools (probado con Apple clang 21.0.0, SDK macOS 26.5). Xcode no hace falta.
- CMake ≥ 3.22 (probado con 4.4.3).
- JUCE 9.0.3, revisión `be29c81492b6151c8ea8d14c840e1311963b3a83`, **dentro** del proyecto
  (`external/JUCE`, ignorado por git):

```bash
git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git external/JUCE
```

## Compilar y probar

Todo en uno (Release universal, CTest y firma ad hoc):

```bash
scripts/build-macos.sh
```

Paso a paso:

```bash
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build/release -j10
```

```bash
ctest --test-dir build/release --output-on-failure
```

Solo el motor, sin JUCE (Clang, GCC o MSVC con C++20):

```bash
cmake -S . -B build/engine -DLABD_BUILD_PLUGIN=OFF && cmake --build build/engine && ctest --test-dir build/engine
```

Por defecto se compila `arm64;x86_64` con mínimo declarado 10.15; el enlazador eleva el
segmento arm64 a 11.0 (resultado real: arm64 ≥ 11.0, x86_64 ≥ 10.15). Artefactos:
`build/release/LABD_artefacts/Release/AU/LAB-D.component` y `.../VST3/LAB-D.vst3`.

Ejecutables de prueba con detalle (filtro opcional por prefijo: `tempo.`, `delay.`,
`channels.`, `eq.`, `lfo.`, `mix.`, `dyn.`, `stability.`, `rt.`; `params.`, `state.`,
`layout.`, `wrapper.`, `editor.`):

```bash
build/release/labd_engine_tests delay.
```

```bash
build/release/labd_plugin_tests_artefacts/Release/labd_plugin_tests editor.
```

Bancos de CPU (Release) y captura del editor:

```bash
build/release/labd_engine_bench 20
```

```bash
build/release/labd_plugin_bench_artefacts/Release/labd_plugin_bench 20
```

```bash
build/release/labd_ui_snapshot_artefacts/Release/labd_ui_snapshot "$PWD/docs/evidence/ui"
```

## Validar y empaquetar (macOS)

`validate-macos.sh` comprueba arquitecturas, mínimos y firma; copia el AU a
`~/Library/Audio/Plug-Ins/Components` (auval solo valida componentes registrados) y
ejecuta `auval -v aufx LbDy Vtpl` en arm64 y en x86_64 (Rosetta); con `LABD_PLUGINVAL`
ejecuta pluginval (nivel 10) sobre el VST3 (arm64 y x86_64) y el AU.

```bash
LABD_PLUGINVAL=/ruta/pluginval.app/Contents/MacOS/pluginval scripts/validate-macos.sh
```

```bash
scripts/package-macos.sh
```

`package-macos.sh` crea en `dist/` un ZIP por formato que conserva el bundle, calcula
SHA-256 y comprueba tras extraerlos que el binario es el compilado y validado.

## Instalar para el smoke

Descomprimir cada ZIP y copiar el bundle (sin instalador):

```bash
ditto LAB-D.component ~/Library/Audio/Plug-Ins/Components/LAB-D.component
```

```bash
ditto LAB-D.vst3 ~/Library/Audio/Plug-Ins/VST3/LAB-D.vst3
```

Los bundles llevan firma ad hoc (prueba local, sin notarizar). Si macOS los bloquea al
venir de otra máquina, quitar la cuarentena:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/LAB-D.component ~/Library/Audio/Plug-Ins/VST3/LAB-D.vst3
```

Verificar el AU y desinstalar:

```bash
auval -v aufx LbDy Vtpl
```

```bash
rm -rf ~/Library/Audio/Plug-Ins/Components/LAB-D.component ~/Library/Audio/Plug-Ins/VST3/LAB-D.vst3
```

En Logic: Ajustes ▸ Plug-in Manager ▸ «VitaPlug LAB-D» debe figurar como validado; si no,
«Reset & Rescan Selection».

## Uso rápido del editor

- Arrastre vertical en los mandos (Mayúsculas: ajuste fino), rueda, flechas/AvPág/Inicio/Fin
  con el mando enfocado, doble clic en el mando = valor inicial.
- Doble clic en una lectura = entrada de texto precisa (`350`, `1/8D`, `1.5k`, `−3.5`).
- ECHO EQ: clic en un nodo o en las teclas 1–6 para elegir banda; arrastrar un nodo mueve
  FREQ (y GAIN cuando la banda la tiene); doble clic en un nodo pone GAIN a 0 dB. La tecla
  de tipo conmuta HIGH CUT/HIGH SHELF con la banda 6.
- LFO: arrastrar sobre la línea L–R de la pantalla fija el PAN base del eco (doble clic: centro).
- ⌘Z / ⇧⌘Z deshacen y rehacen ediciones hechas en el editor (la automatización del host no
  entra en este historial).
- Cada instancia recuerda el tamaño del editor al volver a abrirlo y cuando el host
  restaura el proyecto guardado. Una instancia nueva abre a 1320×600.
- Pie: LED HOST cuando el BPM llega del host; INT cuando se usa el último BPM válido o la
  reserva de 120.
