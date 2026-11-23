# JUCE 9.0.3: el AUv2 omite nonParameterStateChanged. Su notificación
# ClassInfo se envía si se ejecuta el actualizador. Actívalo para ese caso
# en una copia de compilación, sin modificar el checkout de JUCE.
function(labd_forward_au_state_changes)
    set(au_target juce_audio_plugin_client_AU)
    get_target_property(au_sources ${au_target} INTERFACE_SOURCES)
    set(wrapper ${au_sources})
    list(FILTER wrapper INCLUDE REGEX "/juce_audio_plugin_client_AU_1\\.mm$")
    list(LENGTH wrapper wrapper_count)
    if(NOT wrapper_count EQUAL 1)
        message(FATAL_ERROR "LAB-D: no se encontró el wrapper AUv2 esperado de JUCE")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${wrapper}")
    file(READ "${wrapper}" wrapper_code)
    string(REGEX MATCHALL "if \\(flags != 0\\)" conditions "${wrapper_code}")
    list(LENGTH conditions condition_count)
    if(NOT condition_count EQUAL 1)
        message(FATAL_ERROR "LAB-D: revisar la notificación de estado AUv2 contra esta revisión de JUCE")
    endif()
    string(REPLACE "if (flags != 0)" "if (flags != 0 || details.nonParameterStateChanged)"
        wrapper_code "${wrapper_code}")
    set(patched_wrapper "${CMAKE_CURRENT_BINARY_DIR}/labd-au/juce_audio_plugin_client_AU_1.mm")
    file(GENERATE OUTPUT "${patched_wrapper}" CONTENT "${wrapper_code}")

    list(REMOVE_ITEM au_sources "${wrapper}")
    list(APPEND au_sources "${patched_wrapper}")
    set_property(TARGET ${au_target} PROPERTY INTERFACE_SOURCES "${au_sources}")
endfunction()
