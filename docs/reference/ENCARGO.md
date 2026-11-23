# VitaPlug LAB-D 0.1.0 · Prompt único para Claude Code

Implementa **VitaPlug LAB-D**, un delay estéreo de producción y mezcla, con DSP propio y el frontal aprobado por SerxDev. Completa en esta ejecución el código, las pruebas, la autoauditoría, las correcciones y la entrega para smoke de **AUv2 y VST3**. Entrega binarios reales para cada formato que puedas compilar y verificar en el entorno disponible.

Esta especificación y `LAB-D_Frontal.png` son el encargo para construir. El frontal y las funciones descritas ya se pueden implementar. Opto expresamente por omitir una nueva ronda de brainstorming y preguntas de diseño. Si Superpowers está disponible, úsalo para planificar, escribir pruebas con capacidad de detectar fallos, revisar y verificar. Haz esas etapas dentro de la misma ejecución. Si el entorno ofrece Ultracode, puedes usar su flujo de trabajo. Mantén un responsable de integración y una sola candidata final. Resuelve las decisiones reversibles y explica las que afecten al comportamiento. Si un permiso real o una dependencia impide un paso, completa el resto y documenta el bloqueo.

## 1. Proyecto y alcance

- Trabaja en **C++ y JUCE con CMake**. Inspecciona el workspace y sus instrucciones. Aprovecha convenciones y componentes autorizados de VitaPlug si están disponibles. En un proyecto vacío, fija una revisión de JUCE y deja un procedimiento de compilación reproducible.
- Separa el **motor de audio en C++** del wrapper, los parámetros del DAW y el editor. El motor ha de poder compilarse y probarse sin JUCE ni un DAW. Esta separación permite reutilizarlo más adelante en **AI DELAY**. La entrega actual es el delay.
- Prioriza **Audio Unit v2 (`aufx`) para Logic Pro en macOS Apple Silicon** y **VST3 para un host compatible**. En macOS, configura como objetivo el binario universal `arm64` + `x86_64`, con los mínimos de VitaPlug de macOS 11.0 y 10.15 respectivamente si el SDK y JUCE lo permiten. Indica por separado arquitectura compilada, mínimo declarado y entorno realmente probado. Una candidata arm64 verificada tiene prioridad si hay un bloqueo concreto con Intel. Documenta la compilación VST3 en Windows si el entorno ofrece esa posibilidad.
- Soporta **mono → mono** y **estéreo → estéreo**. Expón **mono → estéreo** en los wrappers y hosts que admitan esa disposición y comprueba el resultado. En mono → mono, usa TIME L y realimentación mono en ambos modos, conserva el EQ y deja PAN neutro. Refleja esa disposición en el editor.
- Asigna IDs estables y propios para el plugin, los parámetros y el bundle. Comprueba colisiones con VitaPlug si puedes consultar la familia. Conserva un punto de referencia local de la candidata, con el código exacto que generó los paquetes. Mantén el trabajo y los artefactos dentro del proyecto.

## 2. Tiempo y ruta de audio

| Control | Contrato |
|---|---|
| **TIME L / R y SYNC** | Con SYNC apagado, tiempos libres de 1–4.000 ms. Con SYNC encendido, `1/1`, `1/2`, `1/4`, `1/8`, `1/16` y `1/32`, cada uno normal, con puntillo `D` y de tresillo `T`. Muestra en pantalla la figura y su duración efectiva. Guarda por separado los últimos valores en ms y en figura para cada canal. |
| **LINK** | Los tiempos efectivos L/R coinciden al activarlo. Parte del valor de L. Cualquiera de los dos mandos permite editar el tiempo común. Conserva los ajustes almacenados y documenta la precedencia si ambas automatizaciones llegan a la vez. |
| **FEEDBACK** | 0–98 %, con realimentación acotada. Al 0 % queda un primer eco. La duración y los niveles de las repeticiones deben ser predecibles en CLEAN. |
| **STEREO / PING PONG** | STEREO realimenta cada canal sobre sí mismo. PING PONG cruza la realimentación L↔R. Un impulso que entra por L vuelve primero a L tras `TIME L` y después a R tras `TIME R` adicional. Consigue una alternancia audible también con una fuente mono en un auxiliar estéreo. Documenta cómo entra una señal estéreo a ese modo. |
| **CLEAN / TAPE y DRIVE** | CLEAN con DRIVE al 0 % mantiene las repeticiones limpias. TAPE aporta filtro y saturación de carácter que pueden evolucionar al recircular. DRIVE va de 0 a 100 % y actúa sobre el contenido del eco, perceptible incluso con FEEDBACK al 0 %. Mantén estable el nivel del bucle. Describe y mide la saturación y su coste. |
| **DUCK** | 0–100 %. Un detector interno de la señal seca reduce el nivel de la salida wet mientras suena la fuente y lo recupera suavemente en los huecos. A cero es neutro. El ajuste solo modifica el nivel del wet de salida y deja que el retardo siga recirculando. |
| **MIX y WET ONLY** | MIX lineal de 0–100 %, con extremos exactos. WET ONLY fuerza el 100 % wet para un auxiliar sin escribir sobre MIX. Al salir, recupera el valor de ese parámetro. El mando indica el valor efectivo. |
| **OUTPUT y BYPASS** | OUTPUT de −24 a +12 dB, después de MIX. BYPASS entrega la señal seca a ganancia unitaria, por fuera de OUTPUT, con transición suave. Integra el bypass del host de forma coherente en ambos formatos. |

Lee el BPM del host. Usa el último BPM válido y parte de 120 cuando aún no haya dato. Conserva la figura al cambiar el tempo. Una negra dura `60000 / BPM` ms y `1/N` dura `4/N` negras. Multiplica por `3/2` para puntillo y por `2/3` para tresillo. Dimensiona los búferes antes de procesar para el rango musical declarado. Incluye como mínimo 20–400 BPM y verifica que `1/1D` a 20 BPM alcanza 18 s aunque el modo libre llegue a 4 s.

Implementa lectura interpolada para tiempos fraccionarios y transiciones suaves para saltos de tiempo, SYNC, LINK y cambios de BPM. Usa fundidos entre lecturas o una técnica equivalente y evita saltos de nivel, clics y vaciados abruptos de cola. Mantén la fase del LFO continua durante cambios normales de BPM. Respeta los reinicios explícitos del host y documenta qué ocurre con las colas en stop, seek y ciclo.

La ruta es **líneas de retardo con feedback y color → ECHO EQ → PAN → atenuación DUCK → MIX con la ruta seca → OUTPUT**. Sitúa ECHO EQ y PAN a la salida del bucle. El realce de una banda se aplica una vez al oír una repetición. El paneo desplaza el eco sin modificar la realimentación ni la señal seca. Prioriza latencia de plugin cero. Si una técnica añade latencia, repórtala al host y demuestra la alineación de las ramas seca, wet y bypass sin alterar el tiempo nominal del eco.

## 3. ECHO EQ y modulación

| Banda | Filtro |
|---|---|
| **1** | **LOW CUT**, pasa altos de 12 dB/oct. Frecuencia y Q. |
| **2** | **LOW SHELF**. Frecuencia y ganancia, con pendiente fija. |
| **3, 4 y 5** | **BELL**. Frecuencia, ganancia y Q por banda. |
| **6** | **HIGH CUT / HIGH SHELF**, conmutable. En corte, 12 dB/oct con frecuencia y Q. En shelf, frecuencia y ganancia con pendiente fija. Recuerda el ajuste de ganancia al cambiar de tipo. |

Cada banda tiene `IN` y el bloque tiene `EQ IN` global. Rango de FREQ 20 Hz–20 kHz sujeto a un margen por debajo de Nyquist, GAIN ±18 dB y Q 0,25–8. Mantén estables los filtros al automatizar y modular en todo el rango. La curva debe usar la misma respuesta que el audio. Los nodos permiten editar FREQ y GAIN cuando proceda y elegir la banda. Los mandos inferiores también permiten hacerlo con precisión y editar Q cuando corresponde.

El bloque **LFO** usa una onda común y siete profundidades independientes que se pueden combinar a la vez. Las seis primeras mueven la **frecuencia** de sus respectivas bandas alrededor del ajuste base, en escala logarítmica. La séptima mueve **PAN** del wet. A profundidad 0 % ese destino queda exactamente en su ajuste base. Al 100 % de una banda, usa como punto inicial un recorrido de ±1,5 octavas, acotado al rango válido. Al 100 % de PAN, la onda puede recorrer el estéreo de un extremo al otro. Mantén la imagen original en el centro y la transición suave, incluso con un eco presente en un solo canal.

- **MANUAL**. Formas `SIN`, `TRI`, `SAW`, `SQR` y `RND`. `RND` transita entre valores pseudoaleatorios con semilla reproducible para validar renders. `PHASE` de 0–360°. `RATE` de 0,01–20 Hz libre o sincronizado a figuras y ciclos de 1, 2, 4 y 8 compases. Usa el compás del host para las unidades en compases y 4/4 como reserva.
- **SYNC de RATE** independiente del de TIME. Guarda las dos velocidades por separado. Incluye `1/16`, `1/16T`, `1/32` y `1/32T`. A 120 BPM corresponden a 8, 12, 16 y 24 Hz. El límite de 20 Hz solo se aplica al mando libre. En el mismo proyecto debe funcionar, por ejemplo, `TIME L = 350 ms` con `RATE = 1/32T`.
- **AUTO**. Como punto de partida, seno de un compás. Un seguidor de la señal seca atenúa la profundidad al aumentar el nivel de entrada y la recupera en las colas. Este modo modifica el movimiento y DUCK controla el nivel de las repeticiones. Conserva los ajustes MANUAL al alternar.
- **ON/OFF**. Lleva progresivamente las profundidades efectivas a cero al apagar. Suaviza las aristas de la sierra, cuadrada y cambios de destino sin perder su carácter. Un EQ en bypass no aplica modulación a sus bandas y PAN sigue operativo si está asignado.

## 4. Frontal e integración con el host

Usa **`LAB-D_Frontal.png`** como referencia visual aprobada. **`LAB-D_Interfaz.html`** ilustra la interacción y sus números representan un ajuste de demostración. Conserva el formato horizontal de aproximadamente 2,2:1, el chasis verde oliva, los cantos y orejas con desgaste discreto, los mandos metálicos, los rótulos claros y la firma exacta **Designed by SerxDev** junto a VITAPLUG.

- **Izquierda**. TIME L/R grandes con SYNC y LINK. FEEDBACK, MIX y OUTPUT. Selectores STEREO / **PING PONG** y CLEAN / TAPE. DRIVE y DUCK. Reserva margen para que se lea la G final de PING PONG incluso al mínimo de ventana.
- **Centro**. ECHO EQ tiene la pantalla más grande, con seis nodos y la curva base verde. Muestra en ámbar la respuesta efectiva modulada, a partir de datos reales del DSP. Debajo van las teclas de banda, el tipo, `IN` y FREQ, GAIN y Q. Los rótulos y los valores indican qué control aplica al filtro activo.
- **Derecha**. Pantalla de forma de onda con fase actual y panorama efectivo. ON, AUTO / MANUAL, las cinco formas, RATE, su SYNC y PHASE. Debajo, los siete deslizadores `1`–`6` y `PAN`, con cantidad visible.
- **Pie**. BYPASS, WET ONLY y lectura real del BPM del host o de su valor de reserva. Haz un editor JUCE con mandos operativos, lecturas y escalado nítido para Retina. Trata el arte fijo como fondo y controla dinámicamente botones, curvas, valores y luces. Comprueba legibilidad y ausencia de solapamientos en el tamaño mínimo y el nominal.

Todos los parámetros de audio deben poder automatizarse con nombres y unidades legibles. Implementa gestos de automatización, entrada precisa de valores o teclado y undo desde la edición que admita el host. Guarda y restaura el estado, incluidas asignaciones, valores de los dos modos de SYNC y el valor de MIX conservado durante WET ONLY. Verifica que abrir o cerrar el editor no afecta al procesamiento. Reserva memoria en la preparación, evita bloqueos y E/S en el callback y publica hacia el editor una instantánea de datos de tiempo real. Mide el coste en Release con EQ y LFO activos, idealmente con una y ocho instancias a 48 kHz y 64 y 512 muestras. Identifica el equipo y método de medición.

Toma como **INIT** utilizable `STEREO`, `CLEAN`, TIME libre 350 ms L y 500 ms R, SYNC TIME apagado, LINK apagado, FEEDBACK 30 %, MIX 25 %, OUTPUT 0 dB, DRIVE y DUCK a cero, EQ y LFO sin intervenir y BYPASS/WET ONLY apagados. Deja memorizadas `1/8D` L y `1/4` R como primeras figuras. Registra los demás valores iniciales y deja una ficha reproducible del ajuste que se ve en la maqueta.

## 5. Pruebas medibles

Registra las pruebas automáticas en CTest y ejecútalas. Conserva la causa de cada fallo real que encuentres y la prueba de regresión correspondiente. Usa impulsos, tonos y ruido generados localmente y añade los ajustes usados a la evidencia.

| Área | Criterio mínimo |
|---|---|
| **Retardo y tempo** | A 48 kHz y 120 BPM verifica con impulsos `1/1 = 2.000 ms`, `1/4 = 500 ms` y `1/8D = 375 ms`, además de un tiempo libre. Para tiempos de muestra entera, localiza el primer eco con tolerancia de una muestra. Repite con BPM distintos, cambios de tempo, los extremos de tiempo y BPM ausente. Comprueba ambos SYNC y el recuerdo de sus modos. |
| **Canales y feedback** | Mide canal, tiempo y nivel de varios ecos en STEREO y PING PONG. Incluye impulsos asimétricos, una entrada dual mono, las disposiciones de bus soportadas, FEEDBACK cero y alto, LINK, recorrido completo de PAN y ruta limpia con EQ y LFO apagados. |
| **EQ y LFO** | Mide la respuesta a tonos de cada tipo de banda y confirma que EQ solo afecta al wet. Verifica que la ganancia por vuelta del feedback es la misma con el EQ plano y con un realce de salida. Mide frecuencia de oscilación y actuación de cada uno de los siete destinos por separado y en conjunto. Incluye las cuatro figuras rápidas, `1 BAR` frente a `1/1` en un compás de 3/4, AUTO con y sin señal, bandas apagadas, encendido/apagado y la neutralidad de profundidad cero. |
| **Mezcla y dinámica** | Verifica extremos de MIX, OUTPUT, WET ONLY y BYPASS con señal seca de referencia. Mide DRIVE y el carácter de TAPE a una entrada definida. Comprueba que DUCK reduce el wet durante la fuente y recupera nivel al callar, con la misma cola en el bucle. |
| **Estabilidad y estado** | Automatiza tiempo, switches, EQ, feedback y LFO sobre tonos e impulsos y mide el peor salto/pico con respecto al caso de referencia. Comprueba silencio, datos finitos, colas largas, cambios de configuración, estado serializado y reproducibilidad offline. Repite a 44,1, 48 y 96 kHz y con bloques pequeños y grandes, incluidos 64 y 512. |

## 6. Autoauditoría y validación de formatos

Cuando la primera candidata funcione, haz una revisión de la implementación frente a cada requisito. Sigue el recorrido de una muestra por DSP, wrapper y estado. Revisa la estabilidad de la realimentación, transición de parámetros, tempo y compás, mono y estéreo, tiempo real, automatización, compensación de latencia y legibilidad del editor. Para cada hallazgo registra impacto, evidencia, corrección y verificación. Corrige los hallazgos accionables y repite las pruebas afectadas y la suite final sobre la candidata que vas a empaquetar. Una revisión adicional con Superpowers, si se ejecuta, queda registrada como tal.

Para cada caso registra **PASS**, **FAIL** o **NO EJECUTADO**, acompañado de comando, plataforma y evidencia o motivo. Distingue prueba del motor, prueba integrada del plugin, validación del formato y escucha del productor. Incluye compilador, SDK, JUCE, arquitectura, revisión del código y resultados de los validadores. Para poder reproducir el experimento, anota el modelo, esfuerzo, Ultracode, uso de Superpowers, duración y cualquier intervención humana que haya sido necesaria, según los datos disponibles.

En macOS, compila ambos formatos en Release. Verifica las arquitecturas y mínimos del ejecutable real. Firma ad hoc si procede para la prueba local y documenta el estado de firma. Ejecuta `auval -v aufx <subtipo> <fabricante>` con los identificadores reales del AU entregado. Valida el VST3 con `pluginval`, el validador del SDK o un host VST3 disponible y especifica cuál se usó. Cuando puedas, comprueba el escaneo del AU en Logic y el del VST3 en un host compatible, sin modificar sesiones del usuario. Si falta el entorno macOS, completa el DSP y los demás trabajos verificables y deja los pasos y comandos concretos para construir y validar en el Mac.

## 7. Entrega para smoke

Entrega el código, los comandos de compilación, el resultado de pruebas y autoauditoría, y los paquetes que existan con nombre, versión, plataforma, arquitectura, revisión y SHA-256. Empaqueta `LAB-D.component` y `LAB-D.vst3` de macOS en ZIP separados que conserven los bundles. Comprueba tras extraerlos que contienen el binario que se validó. Incluye pasos de instalación y verificación de cada formato y una captura del editor real si puedes abrirlo. Indica con claridad los pasos pendientes de comprobación.

Deja un `SMOKE.md` breve con una ficha por caso. Cada ficha incluye material, ajustes exactos, resultado esperado y espacio para anotar resultado, versión y captura o bounce si hay un fallo.

1. **Pulso y estéreo**. A 120 BPM, pon `TIME L = 1/8D`, `TIME R = 1/4`, PING PONG, FEEDBACK 35 %, WET ONLY, CLEAN y EQ/LFO apagados. Con un impulso centrado, escucha el primer eco en L a 375 ms y el segundo en R a 875 ms. Después activa el LFO en MANUAL con RATE `1/16T` y PAN al 20 %. Cambia TIME a ms y confirma que RATE conserva su figura.
2. **Guitarra y bajo DI**. Con el mismo pasaje y a volumen comparable, prueba LOW CUT, una campana y HIGH CUT. Modula dos bandas y PAN a distintas profundidades, primero con seno lento y después con `1/16T` y `1/32T`. Compara CLEAN, TAPE y DRIVE.
3. **Huecos en el arreglo**. Usa guitarra o caja y ajusta DUCK para oír el eco en los huecos. Automatiza TIME, GAIN y RATE con la cola sonando. En un Mac Apple Silicon, prueba stop, ciclo y reapertura con búfer de 64 y de 512 muestras cuando el host lo permita.
4. **Sesión y formatos**. Comprueba en Logic con el AU y en un host VST3 con su binario. Prueba inserción y auxiliar, automatización, undo, escala de ventana, cierre del editor, guardado/reapertura, bounce, bypass y uso de varias instancias. Registra cada hallazgo de escucha y marca el smoke como pendiente hasta que se haga en el DAW.

Cierra la ejecución con las rutas de los paquetes reales, la revisión local, qué quedó comprobado y si cada formato está listo para el smoke o requiere una acción concreta. La decisión sonora final se toma por escucha de SerxDev.
