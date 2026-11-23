# LAB-D · Motor de audio y decisiones de diseño

El motor (`src/engine/`) es C++20 sin JUCE ni DAW. Recibe valores planos
(`EngineParams`) y la información de transporte por bloque (`TransportInfo`), procesa
en sitio y publica una telemetría (`EngineTelemetry`) que el wrapper copia en atómicos
para el editor. Se compila y prueba solo (`labd_engine`, `labd_engine_tests`) y está
pensado para reutilizarse en AI DELAY.

## Ruta de la señal

```
entrada ─┬───────────────────────────────────────────────────────────────┐ (seco)
         │                                                               │
         └─► [ + realimentación ] ─► COLOR (CLEAN/TAPE, DRIVE) ─► línea ─┤
                     ▲                                          │       │
                     └──────────── × FEEDBACK (cruzada en PING PONG) ◄──┘
                                                                │
   lectura interpolada (fundido entre lecturas al cambiar TIME) │
                                                                ▼
     compensación DRIVE ─► ECHO EQ ─► PAN ─► DUCK ─► MIX (con el seco) ─► OUTPUT ─► BYPASS ─► salida
```

ECHO EQ, PAN y DUCK están a la salida del bucle: un realce se aplica una sola vez al
oír cada repetición, el paneo desplaza el eco sin tocar la realimentación ni la señal
seca y DUCK solo baja el wet de salida (el bucle sigue recirculando). Latencia del
plugin: 0 muestras (se informa al host). BYPASS entrega la señal seca a ganancia
unitaria, por fuera de OUTPUT, con un fundido de 20 ms; es el parámetro de bypass del
host en AU (`kAudioUnitProperty_BypassEffect`) y VST3 (`kIsBypass`).

El motor procesa en sub-bloques de 16 muestras: LFO, coeficientes del EQ y PAN se
actualizan por sub-bloque (≈0,33 ms a 48 kHz) con interpolación lineal por muestra de
las ganancias; el retardo, la realimentación, el color y la mezcla son por muestra.

## Tiempo y tempo

- **Libre**: 1–4000 ms. **SYNC**: 18 figuras (normal, `D` x3/2, `T` x2/3) de 1/1 a 1/32.
  Una negra dura `60000/BPM` ms y `1/N` dura `4/N` negras. Se guardan por separado, por
  canal, el último valor en ms y la última figura; SYNC elige cuál se usa.
- **BPM**: se usa el último BPM válido del host; hasta recibir uno, 120. Los valores se
  acotan a 20–400 BPM (rango musical declarado); NaN, 0 o negativos se ignoran. Un
  cambio de tempo conserva la figura y recalcula el tiempo.
- **Búfer**: se dimensiona en `prepare` para 18 s (1/1D a 20 BPM) a la frecuencia de
  muestreo actual (potencia de dos: 2^20 muestras/canal a 44,1/48 kHz; 2^21 a 96 kHz).
  Memoria: 8 MB por instancia a 48 kHz, 16 MB a 96 kHz.
- **Lectura**: Hermite (Catmull-Rom) para tiempos fraccionarios; con tiempo entero en
  muestras la lectura es exacta (primer eco en la muestra exacta y nivel 1). Su ganancia
  no supera 1 a ninguna frecuencia ni fracción (prueba `rt.interpolator_gain_bounded`).
- **Transiciones**: un cambio de tiempo (mando, SYNC, LINK, BPM) no salta: el lector
  funde la lectura antigua con la nueva en 50 ms (coseno elevado, ganancias que suman 1).
  Si el objetivo cambia durante un fundido, el siguiente empieza al terminar el actual.
  La cola no se vacía al acortar el tiempo: las repeticiones siguen con el nuevo espaciado.
- **LINK**: los tiempos efectivos L/R coinciden y parten del valor de L (ms o figura,
  según SYNC). Los valores guardados de R no se tocan y vuelven al soltar LINK. En el
  editor cualquiera de los dos mandos edita el tiempo común (escribe en los parámetros
  de L). **Precedencia**: si el host automatiza L y R a la vez con LINK activo, manda L;
  la automatización de R se guarda pero no suena hasta soltar LINK.
- **Mono**: TIME L y realimentación mono en ambos modos (PING PONG se comporta como
  STEREO), EQ completo, PAN neutro. El editor lo refleja (TIME R y PING PONG atenuados,
  «MONO: TIME L», «MONO» en la línea de PAN y en el pie).

## Transporte, reinicios y colas

- **Stop, locate y ciclo**: las líneas no se vacían; la cola sigue sonando y se mezcla
  con lo que llegue después, como en un delay de hardware.
- **LFO sincronizado** (RATE con SYNC o AUTO): al arrancar el transporte o al detectar un
  salto de posición (diferencia > 0,02 negras con la posición esperada: locate, vuelta
  del ciclo) la fase se alinea con la posición musical (`ppq / duración del ciclo`, más
  PHASE). Un cambio normal de BPM no la toca: la fase avanza de forma continua.
- **Reinicio explícito del host** (`reset()` de JUCE: AU `Reset` —el wrapper de JUCE también
  lo emite cuando el host activa `kAudioUnitProperty_BypassEffect`—, VST3 al dejar de
  procesar, o `prepareToPlay`). Puede llegar desde cualquier hilo, así que el wrapper solo
  lo marca y lo aplica **al principio del bloque siguiente, antes de grabar la entrada
  nueva**: las líneas se vacían de forma lógica en O(1) (sin recorrer el búfer en el hilo de
  audio) y la entrada nueva se graba entera desde su primera muestra.
  - **Tras una pausa del host** (más de 10 ms o 4 bloques sin procesar: stop,
    `setProcessing(false)`, inicio de un bounce) el reinicio es completo: la cola anterior
    no vuelve a sonar y EQ, LFO y detectores parten de cero. Un render offline arranca
    siempre desde el mismo estado (reproducible).
  - **En mitad del audio** (por ejemplo, el bypass del AU) la cola anterior deja de
    recircular y se funde en 10 ms a la salida, leída de las muestras anteriores al
    reinicio (`readOld`), sin clic; EQ, LFO y detectores conservan su estado.
  `prepareToPlay` reserva y vacía de verdad.

## FEEDBACK, color y DRIVE

- FEEDBACK 0–98 %. Con 0 % queda un único eco. En CLEAN con DRIVE 0 la repetición `k`
  sale con nivel `FB^k` exacto (medido: error < 5·10⁻⁷ en 50 repeticiones a 98 %).
- El color se aplica a (entrada + realimentación) antes de escribir en la línea: también
  colorea el primer eco (DRIVE se oye con FEEDBACK 0) y se acumula al recircular.
- **CLEAN**: `y = x + DRIVE·(tanh(4x)/4 − x)`; con DRIVE 0 es la identidad exacta.
- **TAPE**: pasa altos de 1 polo (35 Hz) → saturación asimétrica (`tanh(g·x)/g` en la
  parte positiva y `tanh(1,35·g·x)/(1,35·g)` en la negativa, `g = 1,6 + 4,4·DRIVE`) →
  pasa bajos de 1 polo (6,5 kHz, baja a 4,2 kHz con DRIVE). Produce armónicos pares e
  impares y oscurece las repeticiones vuelta a vuelta.
- **Estabilidad**: las dos curvas tienen pendiente 1 en el origen y ≤ 1 en todo el rango,
  los filtros de un polo tienen |H| ≤ 1 y la lectura interpolada también. La ganancia
  incremental del bucle es ≤ FEEDBACK ≤ 0,98: las repeticiones siempre decaen, con
  cualquier DRIVE. Una muestra no finita en el bucle se sustituye por 0.
- **Compensación**: como la curva no puede amplificar dentro del bucle, el eco audible
  recibe fuera del bucle una ganancia que mantiene el primer armónico de un seno a
  −12 dBFS al mismo nivel que en CLEAN sin DRIVE (tabla calculada en `prepare`). Es 1
  exacta con CLEAN y DRIVE 0 y no cambia la realimentación.
- Los cambios CLEAN↔TAPE, de DRIVE y de FEEDBACK se suavizan (30/30/20 ms).

Medida (seno de 1 kHz a −6 dBFS, FEEDBACK 0, 100 % wet; prueba `mix.drive_and_tape_character`):

| Ajuste | Fundamental | THD | H2 | H3 |
|---|---|---|---|---|
| CLEAN DRIVE 0 | 0,00 dB | 0,000 % | — | — |
| CLEAN DRIVE 50 | −1,30 dB | 6,2 % | — | −24,4 dB |
| CLEAN DRIVE 100 | −3,24 dB | 17,3 % | — | −15,5 dB |
| TAPE DRIVE 0 | −1,28 dB | 6,2 % | −30,8 dB | −25,2 dB |
| TAPE DRIVE 50 | −3,66 dB | 17,5 % | −25,5 dB | −15,8 dB |
| TAPE DRIVE 100 | −5,01 dB | 22,8 % | −26,7 dB | −13,5 dB |

A −12 dBFS el eco sale a 0,00 dB en CLEAN con cualquier DRIVE y entre −0,12 y −0,24 dB
en TAPE (los filtros del bucle); a −30 dBFS la compensación máxima es +3,9 dB (TAPE
DRIVE 100). Coste: ver `docs/PRUEBAS.md` (banco de CPU).

## STEREO y PING PONG

- **STEREO**: cada canal se realimenta sobre sí mismo con su tiempo.
- **PING PONG**: la realimentación se cruza L↔R. La entrada estéreo se suma a mono
  `(L + R)/2` y entra solo por la línea L. Un impulso vuelve primero a L tras TIME L y
  después a R tras TIME R adicional (L 375 ms, R 875 ms con 1/8D–1/4 a 120 BPM). Así
  una fuente mono en un auxiliar estéreo alterna de forma audible; una fuente estéreo
  pierde su anchura propia dentro de los ecos (la alternancia la sustituye). Una fuente
  centrada entra a nivel 1; una solo en un canal, a −6 dB.
- El cambio entre modos se funde en 30 ms.

## DUCK

Detector de pico con retención sobre la señal seca (máximo de |L| y |R|; una muestra no
finita cuenta como silencio, para que un Inf o NaN de entrada no deje el detector
inutilizado): ataque 2 ms,
retención 30 ms, liberación 60 ms. Zona de actuación de −45 dBFS (nada) a −30 dBFS
(todo). La ganancia del wet baja hasta `−36 dB × DUCK` con ataque de 10 ms y se
recupera con 100 ms. DUCK 0 es neutro bit a bit. Solo atenúa el wet de salida: la cola
del bucle es la misma (medido: −36,0 dB durante la fuente con DUCK 100 %, −0,1 dB en el
hueco 0,6–1,4 s después, cola idéntica).

## MIX, WET ONLY y OUTPUT

MIX lineal `seco·(1−m) + wet·m`, con extremos exactos (MIX 0 devuelve la entrada bit a
bit). WET ONLY fuerza m = 1 sin escribir en MIX; al salir se recupera el valor guardado
(el mando muestra el valor efectivo, 100 %, y queda inactivo). OUTPUT −24…+12 dB después
de MIX.

**BYPASS** (el del plugin y el del host, que es el mismo parámetro): la salida pasa al seco
exacto con una rampa de 20 ms; con la misma rampa la entrada deja de grabarse en el bucle,
y cuando el bypass es completo la cola se vacía (en silencio). Al salir, el delay arranca
limpio: no suenan ecos de lo que entró durante el bypass ni la cola anterior. Así AU (donde
JUCE emite además un `Reset` al activar el bypass) y VST3 se comportan igual. Los parámetros se cuantifican al leerlos (0,001 ms y 0,001 dB) para que la
normalización del host no introduzca 99,9999 ms o 3·10⁻⁶ dB.

## ECHO EQ

Seis secciones de filtro de estado variable TPT (Simper), estables con coeficientes
variables en el tiempo. Diseño: LOW CUT y HIGH CUT de 12 dB/oct con Q; LOW SHELF y HIGH
SHELF con pendiente fija (Q 0,707); BELL con GAIN y Q. FREQ 20 Hz–20 kHz acotada a
0,45·fs (19,845 kHz a 44,1 kHz), GAIN ±18 dB, Q 0,25–8. FREQ (en escala log), GAIN y Q se
suavizan con 20 ms; IN de banda, EQ IN y el cambio HIGH CUT↔HIGH SHELF se funden en 20 ms
(la banda baja a cero, cambia de tipo y vuelve). La respuesta que dibuja el editor sale
de los mismos coeficientes (`designSvf` + `svfResponse`): medido contra tonos, la curva y
el audio coinciden al milésimo de dB en los nueve casos medidos (tolerancia de la prueba: 0,05 dB).

## LFO

- Una onda común y siete profundidades (bandas 1–6 y PAN) combinables. Bandas: la
  frecuencia se mueve en escala logarítmica `f·2^(1,5·profundidad·onda)` (±1,5 octavas al
  100 %), acotada al rango válido. PAN: `PAN base + profundidad·onda`, acotado a ±1 (al
  100 % recorre de extremo a extremo). Profundidad 0 deja el destino exactamente en su
  valor base (salida idéntica bit a bit a LFO apagado).
- Formas SIN, TRI, SAW, SQR y RND. RND transita (coseno) entre valores pseudoaleatorios
  que dependen solo del índice de ciclo y de una semilla fija: dos renders son idénticos.
- RATE libre 0,01–20 Hz o sincronizado (figuras y 1, 2, 4, 8 compases con el compás del
  host, 4/4 de reserva). Velocidad libre y figura se guardan por separado; el SYNC de RATE
  es independiente del de TIME.
- AUTO: seno de un compás con fase 0. Un seguidor de la señal seca (ataque 30 ms,
  liberación 200 ms, misma zona −45…−30 dBFS) reduce la profundidad hasta un 15 % con
  señal y la recupera en las colas. Conserva forma, RATE y PHASE de MANUAL.
- ON/OFF: rampa de 80 ms de las profundidades efectivas; cambio de profundidad suavizado
  50 ms; aristas de SAW/SQR y cambios de forma suavizados con un polo de 2 % del periodo
  (1–5 ms): la cuadrada sigue llegando a ±1.
- Con el EQ apagado (EQ IN o IN de banda) la modulación no actúa sobre esas bandas; PAN
  sigue modulando.

## PAN del wet

Ley de potencia constante que traslada un canal al otro: en el centro es la identidad
(se conserva la imagen original); hacia R, `L' = L·cos θ`, `R' = R + L·sin θ` con
`θ = |p|·π/2` (y simétrico hacia L). Un eco presente solo en L recorre el estéreo de
extremo a extremo sin desaparecer y conserva su potencia. Contrapartida asumida: un eco
idéntico en L y R (fuente mono en STEREO con tiempos iguales) suma potencia al
desplazarse, +2,3 dB en ±0,5 y +3,0 dB en el extremo, con el pico de un canal hasta +6 dB;
con PAN del LFO al 100 % se oye como un ligero vaivén de nivel. Fijado por la prueba
`mix.pan_law_correlated_echo`.

## Tiempo real

`process()` no reserva memoria (prueba con contador de `operator new`), no bloquea y no
hace E/S. El wrapper lee los parámetros como atómicos, usa `ScopedNoDenormals`, publica
la telemetría en atómicos y aplaza `reset()` al hilo de audio. El editor solo lee esa
telemetría y los valores de los parámetros: abrirlo o cerrarlo no cambia el audio
(prueba bit a bit).
