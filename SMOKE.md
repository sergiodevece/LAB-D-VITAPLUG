# LAB-D 0.1.0 · Smoke en DAW

Estado: **PENDIENTE** hasta que SerxDev lo haga en el DAW. La decisión sonora final es
por escucha. Para cada fallo, anota resultado, versión/revisión y adjunta captura o bounce.

Instalación previa (macOS): ver `README.md` → «Instalar para el smoke». Comprueba en
Logic que aparece **VitaPlug ▸ LAB-D** (Audio Units) y en el host VST3 **VitaPlug LAB-D**.
INIT = cargar el plugin nuevo (sin preset).

---

## 1 · Pulso y estéreo

- **Material**: impulso o clic centrado (mono en pista estéreo o en auxiliar estéreo), 120 BPM.
- **Ajustes**: INIT → SYNC on, TIME L `1/8D`, TIME R `1/4`, PING PONG, FEEDBACK 35 %,
  WET ONLY, CLEAN, EQ IN off, LFO off.
- **Esperado**: primer eco en **L a 375 ms**, segundo en **R a 875 ms**, tercero en L a
  1250 ms (35 % del anterior cada vez). La pantalla muestra `1/8D · 375 ms` y `1/4 · 500 ms`.
- Después: LFO ON, MANUAL, RATE SYNC `1/16T`, DEPTH PAN 20 % → los ecos tiemblan en el
  panorama a 12 Hz. Apaga SYNC de TIME (vuelven 350/500 ms) → **RATE sigue en `1/16T`**.
- Resultado: ☐ OK ☐ Fallo — versión/revisión: ________ — notas/captura: ______________

## 2 · Guitarra y bajo DI

- **Material**: el mismo pasaje de guitarra DI y de bajo DI, a volumen comparable.
- **Ajustes**: INIT → TIME libre 350/500 ms, FEEDBACK 40 %, MIX 30 %.
  1. EQ: banda 1 LOW CUT IN a ~150 Hz; banda 3 BELL +6 dB a ~1,5 kHz Q 1; banda 6 HIGH
     CUT IN a ~5 kHz. La curva verde y el sonido deben coincidir (solo cambian los ecos;
     el seco no).
  2. LFO ON, SIN, RATE libre 0,3 Hz: DEPTH 3 = 60 %, DEPTH 6 = 30 %, PAN = 40 %. Después
     RATE SYNC `1/16T` y `1/32T`. La curva ámbar sigue a la modulación.
  3. Compara CLEAN, TAPE y DRIVE 0/50/100 %: el nivel percibido de los ecos debe
     mantenerse parecido; TAPE oscurece las repeticiones vuelta a vuelta.
- **Esperado**: sin clics al mover FREQ/GAIN/Q ni al cambiar de forma; HIGH CUT↔HIGH SHELF
  (tecla de tipo con la banda 6) sin saltos y recordando GAIN.
- Resultado: ☐ OK ☐ Fallo — versión/revisión: ________ — notas/captura: ______________

## 3 · Huecos en el arreglo

- **Material**: guitarra rítmica o caja con silencios.
- **Ajustes**: INIT → FEEDBACK 45 %, MIX 35 %, DUCK de 0 a 100 %.
- **Esperado**: con DUCK alto el eco casi desaparece mientras suena la fuente y vuelve
  suavemente en los huecos; la cola no se acorta.
- Automatiza TIME, GAIN de una banda y RATE con la cola sonando: sin clics ni vaciados.
- En Apple Silicon: stop, ciclo y reapertura del proyecto con búfer de **64** y **512**
  muestras. Stop/locate/ciclo no cortan la cola (salvo que el host emita un Reset); el
  LFO sincronizado vuelve a su fase musical al arrancar.
- Resultado: ☐ OK ☐ Fallo — versión/revisión: ________ — notas/captura: ______________

## 4 · Sesión y formatos

- **Logic (AU)** y **host VST3** con su binario: inserción en pista e inserción en
  auxiliar (WET ONLY), pista mono (mono→mono y mono→estéreo si el host lo ofrece).
- Comprueba: automatización (lectura/escritura/touch), undo (⌘Z en el editor y undo del
  host), escala de ventana (mínimo 1100×500, nominal 1320×600), cerrar y abrir el editor
  durante la reproducción, guardar y reabrir la sesión, bounce/export (dos bounces
  iguales con RND), bypass del host y BYPASS del plugin, varias instancias.
- Resultado AU: ☐ OK ☐ Fallo — versión/revisión: ________ — notas: ______________
- Resultado VST3: ☐ OK ☐ Fallo — host: ________ — notas: ______________

---

Registro de hallazgos de escucha (fecha, caso, descripción, captura/bounce):

| Fecha | Caso | Hallazgo | Archivo |
|---|---|---|---|
| | | | |
