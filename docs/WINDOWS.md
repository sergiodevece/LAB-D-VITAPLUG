# LAB-D · VST3 en Windows (procedimiento, no ejecutado en esta entrega)

El entorno de esta entrega es macOS sin acceso a Windows, así que el VST3 de Windows no
se ha compilado ni probado. El código no usa nada específico de macOS fuera de JUCE y el
motor es C++20 estándar. Pasos previstos en un PC con Windows 10/11 x64:

1. Instalar Visual Studio 2022 (carga «Desarrollo para el escritorio con C++»), CMake ≥ 3.22
   y Git.
2. Clonar el proyecto y JUCE 9.0.3 dentro de él:

   ```bat
   git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git external\JUCE
   git -C external\JUCE rev-parse HEAD
   ```
   Debe imprimir `be29c81492b6151c8ea8d14c840e1311963b3a83`.
3. Configurar, compilar y probar (el AU se omite solo en Windows):

   ```bat
   cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
   cmake --build build-win --config Release --parallel
   ctest --test-dir build-win -C Release --output-on-failure
   ```
4. El bundle queda en `build-win\LABD_artefacts\Release\VST3\LAB-D.vst3`. Validarlo con
   pluginval (Windows) a nivel 10:

   ```bat
   pluginval.exe --strictness-level 10 --validate-in-process --validate build-win\LABD_artefacts\Release\VST3\LAB-D.vst3
   ```
5. Instalación manual: copiar la carpeta `LAB-D.vst3` en `C:\Program Files\Common Files\VST3\`.

Pendiente: compilar, ejecutar CTest y pluginval en Windows y anotar los resultados en
`docs/PRUEBAS.md`. Si `FORMATS AU VST3` diera problemas en el generador de Visual Studio,
JUCE ignora AU fuera de Apple; no hace falta cambiar el `CMakeLists.txt`.
