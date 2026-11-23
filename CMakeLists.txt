cmake_minimum_required(VERSION 3.22)

# macOS: binario universal. El mínimo declarado es 10.15; el enlazador eleva el
# segmento arm64 a 11.0 (primer macOS con Apple Silicon), así que el resultado
# es arm64 >= 11.0 y x86_64 >= 10.15. Se comprueba con vtool en validate-macos.sh.
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
    endif()
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
    endif()
endif()

project(LABD VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "No se admiten builds dentro del árbol de fuentes. Usa: cmake -S . -B build/release")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LABD_BUILD_PLUGIN "Compila el plugin JUCE (AU/VST3), sus pruebas y herramientas" ON)

enable_testing()

#==============================================================================
# Motor de audio: C++ puro, sin JUCE ni DAW (reutilizable en AI DELAY).
add_library(labd_engine STATIC
    src/engine/TempoMath.cpp
    src/engine/Svf.cpp
    src/engine/EchoEq.cpp
    src/engine/DelayEngine.cpp)
target_include_directories(labd_engine PUBLIC src)
target_compile_features(labd_engine PUBLIC cxx_std_20)
if(NOT MSVC)
    target_compile_options(labd_engine PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

add_executable(labd_engine_tests
    tests/engine/TestMain.cpp
    tests/engine/AllocationGuard.cpp
    tests/engine/TempoTests.cpp
    tests/engine/DelayTests.cpp
    tests/engine/ChannelTests.cpp
    tests/engine/EqTests.cpp
    tests/engine/LfoTests.cpp
    tests/engine/MixDynamicsTests.cpp
    tests/engine/StabilityTests.cpp)
target_include_directories(labd_engine_tests PRIVATE tests)
target_link_libraries(labd_engine_tests PRIVATE labd_engine)

foreach(group tempo delay channels eq lfo mix dyn stability rt)
    add_test(NAME engine.${group} COMMAND labd_engine_tests "${group}.")
    set_tests_properties(engine.${group} PROPERTIES LABELS "engine")
endforeach()

# Banco de CPU del motor (Release). No forma parte de CTest.
add_executable(labd_engine_bench tools/EngineBench.cpp)
target_link_libraries(labd_engine_bench PRIVATE labd_engine)

#==============================================================================
if(LABD_BUILD_PLUGIN)
    # JUCE 9.0.3 (be29c81492b6151c8ea8d14c840e1311963b3a83), dentro del proyecto.
    set(LABD_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE" CACHE PATH "JUCE 9.0.3")
    if(NOT EXISTS "${LABD_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "JUCE no está en ${LABD_JUCE_DIR}.\n"
            "Ejecuta: git clone --depth 1 --branch 9.0.3 https://github.com/juce-framework/JUCE.git external/JUCE")
    endif()
    file(STRINGS "${LABD_JUCE_DIR}/CMakeLists.txt" _juce_project REGEX "^project\\(JUCE VERSION")
    if(NOT _juce_project MATCHES "9\\.0\\.3")
        message(WARNING "LAB-D está fijado a JUCE 9.0.3; encontrado: ${_juce_project}")
    endif()
    add_subdirectory("${LABD_JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

    juce_add_binary_data(labd_assets
        HEADER_NAME LabdAssets.h
        NAMESPACE LabdAssets
        SOURCES assets/LAB-D_Frontal.png)

    set(LABD_WRAPPER_SOURCES
        src/plugin/Parameters.cpp
        src/plugin/PluginProcessor.cpp
        src/ui/LabdLookAndFeel.cpp
        src/ui/EqDisplay.cpp
        src/ui/LfoDisplay.cpp
        src/ui/PluginEditor.cpp)

    set(LABD_JUCE_DEFINITIONS
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        LABD_VERSION_STRING="${PROJECT_VERSION}")

    juce_add_plugin(LABD
        VERSION ${PROJECT_VERSION}
        COMPANY_NAME "VitaPlug"
        COMPANY_WEBSITE ""
        BUNDLE_ID com.vitaplug.labd
        PLUGIN_MANUFACTURER_CODE Vtpl
        PLUGIN_CODE LbDy
        FORMATS AU VST3
        PRODUCT_NAME "LAB-D"
        DESCRIPTION "VitaPlug LAB-D stereo delay"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        AU_MAIN_TYPE kAudioUnitType_Effect
        VST3_CATEGORIES Fx Delay
        NEEDS_WEB_BROWSER FALSE
        NEEDS_CURL FALSE)

    target_sources(LABD PRIVATE ${LABD_WRAPPER_SOURCES})
    target_include_directories(LABD PRIVATE src)
    target_compile_definitions(LABD PUBLIC ${LABD_JUCE_DEFINITIONS})
    target_link_libraries(LABD
        PRIVATE
            labd_engine
            labd_assets
            juce::juce_audio_processors
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Pruebas integradas del plugin (procesador y editor sin DAW).
    juce_add_console_app(labd_plugin_tests PRODUCT_NAME "labd_plugin_tests")
    target_sources(labd_plugin_tests PRIVATE tests/plugin/PluginTests.cpp tests/engine/AllocationGuard.cpp ${LABD_WRAPPER_SOURCES})
    target_include_directories(labd_plugin_tests PRIVATE src tests)
    target_compile_definitions(labd_plugin_tests PRIVATE ${LABD_JUCE_DEFINITIONS} JucePlugin_Name="LAB-D" JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(labd_plugin_tests
        PRIVATE labd_engine labd_assets juce::juce_audio_processors juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    foreach(group params state layout wrapper editor)
        add_test(NAME plugin.${group} COMMAND labd_plugin_tests "${group}.")
        set_tests_properties(plugin.${group} PROPERTIES LABELS "plugin")
    endforeach()

    # Banco de CPU del procesador completo (1 y 8 instancias). Fuera de CTest.
    juce_add_console_app(labd_plugin_bench PRODUCT_NAME "labd_plugin_bench")
    target_sources(labd_plugin_bench PRIVATE tools/PluginBench.cpp ${LABD_WRAPPER_SOURCES})
    target_include_directories(labd_plugin_bench PRIVATE src)
    target_compile_definitions(labd_plugin_bench PRIVATE ${LABD_JUCE_DEFINITIONS} JucePlugin_Name="LAB-D")
    target_link_libraries(labd_plugin_bench
        PRIVATE labd_engine labd_assets juce::juce_audio_processors juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)

    # Captura del editor real renderizado fuera de pantalla.
    juce_add_console_app(labd_ui_snapshot PRODUCT_NAME "labd_ui_snapshot")
    target_sources(labd_ui_snapshot PRIVATE tools/UiSnapshot.cpp ${LABD_WRAPPER_SOURCES})
    target_include_directories(labd_ui_snapshot PRIVATE src)
    target_compile_definitions(labd_ui_snapshot PRIVATE ${LABD_JUCE_DEFINITIONS} JucePlugin_Name="LAB-D")
    target_link_libraries(labd_ui_snapshot
        PRIVATE labd_engine labd_assets juce::juce_audio_processors juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)
endif()
