# LAB-D 0.1.0 · Autoauditoría

Revisión de la primera candidata funcional frente a cada requisito del encargo,
siguiendo el recorrido de una muestra por el motor (`DelayEngine::process`), el wrapper
(`LabdProcessor::runEngine`, parámetros, transporte, telemetría) y el estado
(`get/setStateInformation`). Para cada hallazgo: impacto, evidencia, corrección y
verificación. Al final, la revisión independiente con Superpowers.

## A. Hallazgos del desarrollo y de la autoauditoría

| # | Hallazgo | Impacto | Evidencia | Corrección | Verificación (regresión) |
|---|---|---|---|---|---|
| A1 | La tabla de RATE colocaba 1 BAR (4 negras en 4/4) antes que 1/1D (6 negras): el mando no iba de lento a rápido | Orden confuso del mando | `tempo.rate_fast_figures` fallaba | Orden `8,4,2 BAR, 1/1D, 1 BAR, 1/1…`; 1 BAR = 1/1 en 4/4 | `tempo.rate_fast_figures` PASS |
| A2 | DUCK recuperaba el wet demasiado tarde en los huecos (−1,0 dB a 0,6–1,4 s) | El eco no «respira» en huecos cortos | `dyn.duck_reduces_and_recovers`: −1,03 dB | Detector con retención 30 ms y liberación 60 ms; zona −45…−30 dBFS; ganancia 10/100 ms | −0,10 dB en el hueco, −36 dB con la fuente |
| A3 | AUTO devolvía la profundidad del LFO muy despacio en las colas (escala 0,23 a 1 s) | AUTO casi no modulaba en colas | `lfo.auto_mode_follower` | Liberación del seguidor 500 → 200 ms | escala 1,000 a 1 s de silencio |
| A4 | Suavizado de aristas del LFO demasiado largo: la sierra perdía un 11 % de recorrido | Carácter de SAW atenuado | `lfo.square_saw_edges_smoothed`: −0,888 | 2 % del periodo, 1–5 ms | SAW −0,921…0,981, SQR ±1 |
| A5 | La normalización del host no devuelve el valor exacto: 0 dB → 3·10⁻⁶ dB, 100 ms → 99,9999 ms | OUTPUT no era ganancia unitaria exacta; ecos a fracción de muestra en tiempos redondos | `params.init_values`, `layout.mono_to_mono_uses_time_l` | Cuantificación al leer (0,001 ms / 0,001 dB) y porcentajes divididos por 100 | ambas pruebas PASS; 100 ms = 4800 muestras exactas |
| A6 | `reset()` puede llegar fuera del hilo de audio (AU `Reset`, también al activar el bypass del AU) y vaciar el búfer mientras se procesa: carrera de datos y corte brusco de la cola | Clic o fallo raro en el host | Lectura de `juce_audio_plugin_client_AU_1.mm` (Reset al activar bypass) | `reset()` solo marca; el bloque siguiente funde el wet 10 ms y vacía las líneas en O(1) | `wrapper.reset_from_other_thread`, `stability.soft_reset_fades_then_clears` |
| A7 | «HIGH SHELF ↔» no cabía en la tecla de tipo (163 px en 148) y «400.0 BPM» no cabía en la lectura del pie | Texto cortado en el editor | `editor.text_fits_and_no_overlap` | Tecla con letra ajustada (mín. 17 px de diseño, sin la flecha); lectura del pie más ancha | PASS; «HIGH SHELF» a 19,5 px |
| A8 | Los controles inactivos (TIME R y PING PONG en mono, MIX con WET ONLY, GAIN/Q no aplicables) no se atenuaban: `Graphics::setOpacity` solo afecta al color actual | Disposición mono y controles no aplicables no se distinguían | Captura `editor_mono_1320.png` | Capas de transparencia (`beginTransparencyLayer`) | captura revisada |
| A9 | Con el `UndoManager` conectado al `AudioProcessorValueTreeState`, la automatización del host entraba en el historial y, con varios parámetros automatizados, la transacción abierta crecía sin límite | Memoria creciente en sesiones largas; undo deshacía automatización | Lectura de `juce_UndoManager.cpp` (coalescencia solo con la última acción) | `EditHistory`: solo gestos del plugin (begin/end), agrupados de pulsar a soltar | `editor.keyboard_and_double_click_are_host_gestures`, `editor.eq_band_controls_and_undo` |
| A10 | En JUCE 9 `SliderParameterAttachment` solo abre gesto al arrastrar: el teclado y el doble clic del mando cambiaban el parámetro sin gesto | El host no registraba esas ediciones en touch/latch ni entraban en el undo | prueba de undo fallida; `juce_ParameterAttachments.cpp` | `Slider::ScopedDragNotification` en teclado y doble clic | `editor.keyboard_and_double_click_are_host_gestures` (2 gestos abiertos/cerrados) |
| A11 | El editor leía la banda seleccionada del `ValueTree`, que `setStateInformation` puede sustituir desde otro hilo en algunos hosts | Carrera de datos posible | revisión del código | Banda en un `std::atomic<int>`, sincronizada solo en get/setState | `state.roundtrip_all_values` |
| A12 | La pantalla ECHO EQ recalculaba 3600 diseños de filtro por fotograma y repintaba a 30 Hz aunque nada cambiara | Coste de CPU del editor | revisión del código | Coeficientes una vez por refresco; repintado solo con cambios o con el LFO modulando | pruebas del editor PASS |
| A13 | DRIVE bajaba mucho el eco audible (TAPE DRIVE 100: −9,3 dB a −6 dBFS; techo ≈ −15,6 dBFS), porque la curva no puede amplificar dentro del bucle | Al comparar CLEAN/TAPE/DRIVE «a volumen comparable» el eco se hunde | `mix.drive_and_tape_character` | Compensación fuera del bucle calibrada con el primer armónico de un seno a −12 dBFS (tabla en `prepare`) | `mix.drive_keeps_echo_level_at_minus_12`: CLEAN 0,00 dB, TAPE ≥ −0,24 dB |
| A14 | `TestFramework.h` usaba `__attribute__` sin guarda: el procedimiento de Windows (MSVC) no compilaría las pruebas | Bloquea CTest en Windows | revisión del código | Guarda `__GNUC__/__clang__` | compila en macOS; Windows pendiente |

Fallos de las propias pruebas (no del producto), corregidos y anotados: expectativa del
HIGH CUT a 8 kHz tomada del prototipo analógico (−24,1 dB) en lugar de la respuesta
bilineal real (−25,7 dB; la curva del editor coincide con el audio); métrica de clics
normalizada por el RMS local que se disparaba en pasajes casi silenciosos (sustituida por
la segunda diferencia absoluta frente a la referencia); búfer de prueba demasiado corto
para el eco de R a 500 ms.

## B. Recorrido por requisitos (resumen)

| Requisito | Estado | Dónde se comprueba |
|---|---|---|
| Motor sin JUCE, wrapper separado | Cumple | `labd_engine` + `labd_engine_tests` compilan sin JUCE |
| AU v2 `aufx` + VST3, universal, mínimos 11.0 / 10.15 | Cumple | `vtool`, auval, pluginval |
| mono→mono, estéreo→estéreo, mono→estéreo | Cumple (AU: 1-1, 1-2, 2-2 publicados) | `layout.*`, auval |
| TIME 1–4000 ms, 18 figuras, memoria ms/figura, LINK | Cumple | `delay.*`, `editor.time_knobs_follow_sync_and_link` |
| FEEDBACK acotado y predecible | Cumple | `channels.feedback_zero_and_high`, `rt.interpolator_gain_bounded` |
| STEREO / PING PONG y entrada estéreo documentada | Cumple | `channels.*`, `docs/DSP.md` |
| CLEAN/TAPE/DRIVE medidos, nivel del bucle estable | Cumple | `mix.drive_*`, `stability.silence_and_finite` |
| DUCK solo en el wet de salida | Cumple | `dyn.duck_reduces_and_recovers` |
| MIX lineal exacto, WET ONLY sin escribir MIX, OUTPUT, BYPASS | Cumple | `mix.*`, `state.wet_only_restores_mix`, `wrapper.bypass_latency_tail` |
| BPM del host, último válido, 120 de reserva, 20–400, 18 s | Cumple | `delay.*`, `wrapper.host_tempo_and_reserve` |
| Transiciones suaves (TIME, SYNC, LINK, BPM, switches) | Cumple | `delay.time_jump_crossfade_no_click`, `stability.automation_no_clicks` |
| Ruta y latencia 0 | Cumple | `eq.only_wet_and_loop_gain_unchanged`, `wrapper.bypass_latency_tail` |
| ECHO EQ (tipos, IN, EQ IN, rangos, estabilidad, curva = audio) | Cumple | `eq.*` |
| LFO (7 destinos, ±1,5 oct, PAN completo, formas, RND, RATE, AUTO, ON/OFF) | Cumple | `lfo.*` |
| Frontal y editor (tamaños, legibilidad, mono, WET ONLY) | Cumple en capturas y pruebas; escucha y uso real pendientes | `editor.*`, `docs/evidence/ui/` |
| Automatización, gestos, texto, teclado, undo, estado | Cumple | `params.*`, `state.*`, `editor.*`, pluginval |
| Tiempo real y editor sin efecto en el audio | Cumple | `rt.*`, `wrapper.realtime_no_allocation`, `wrapper.editor_open_close_does_not_affect_audio` |

## C. Revisión independiente (Superpowers)

Pendiente de incorporar al terminar la revisión.
