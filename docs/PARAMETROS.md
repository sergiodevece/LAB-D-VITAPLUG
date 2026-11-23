# LAB-D · Parámetros e INIT

54 parámetros automatizables con ID estable (versión de parámetro 1). Los IDs no se
renombran: las sesiones guardadas dependen de ellos. En el host aparecen con nombre
legible y unidad; el texto admite entrada precisa (por ejemplo `1.5k`, `−3.5`, `L30`, `1/8D`).

Identificadores del plugin: fabricante `Vtpl`, subtipo AU `LbDy` (`aufx`), bundle
`com.vitaplug.labd`, nombre `LAB-D`. Comprobado sin colisión con los AU VitaPlug
instalados (`D67L`, `Gn80`, `HhMx`, `HmR1`, `Lb2C`, `LbTr`, `Lbq8`, `Imsp`, `DlAe`, `Dlab`).

## Tiempo

| ID | Nombre en el host | Rango | INIT | Notas |
|---|---|---|---|---|
| `time_sync` | Time Sync | Off/On | Off | SYNC de TIME (común a L y R) |
| `time_link` | Time Link | Off/On | Off | LINK: R efectivo = L |
| `time_l_ms` | Time L | 1–4000 ms (log) | 350 ms | valor libre memorizado |
| `time_r_ms` | Time R | 1–4000 ms (log) | 500 ms | valor libre memorizado |
| `time_l_note` | Time L Note | 18 figuras | 1/8D | figura memorizada |
| `time_r_note` | Time R Note | 18 figuras | 1/4 | figura memorizada |

Figuras de TIME (orden del mando, de corta a larga): 1/32T, 1/32, 1/16T, 1/32D, 1/16,
1/8T, 1/16D, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/1T, 1/2D, 1/1, 1/1D.

## Bucle, salida y wet

| ID | Nombre | Rango | INIT |
|---|---|---|---|
| `feedback` | Feedback | 0–98 % | 30 % |
| `stereo_mode` | Stereo Mode | Stereo / Ping Pong | Stereo |
| `color` | Color | Clean / Tape | Clean |
| `drive` | Drive | 0–100 % | 0 % |
| `duck` | Duck | 0–100 % | 0 % |
| `mix` | Mix | 0–100 % (lineal) | 25 % |
| `wet_only` | Wet Only | Off/On | Off |
| `output` | Output | −24…+12 dB | 0 dB |
| `bypass` | Bypass | Off/On | Off (es el bypass del host) |
| `pan` | Echo Pan | L100…C…R100 | C |

`pan` es el PAN base del eco (destino 7 del LFO). En el editor se ajusta arrastrando
sobre la línea L–R de la pantalla del LFO (doble clic lo centra).

## ECHO EQ

| ID | Nombre | Rango | INIT |
|---|---|---|---|
| `eq_on` | EQ In | Off/On | On |
| `eq1_on`, `eq1_freq`, `eq1_q` | EQ 1 Low Cut In/Freq/Q | 20 Hz–20 kHz, Q 0,25–8 | Off · 30 Hz · 0,707 |
| `eq2_on`, `eq2_freq`, `eq2_gain` | EQ 2 Low Shelf In/Freq/Gain | ±18 dB | On · 120 Hz · 0 dB |
| `eq3_on`, `eq3_freq`, `eq3_gain`, `eq3_q` | EQ 3 Bell | | On · 400 Hz · 0 dB · 1,00 |
| `eq4_on`, `eq4_freq`, `eq4_gain`, `eq4_q` | EQ 4 Bell | | On · 1,5 kHz · 0 dB · 1,00 |
| `eq5_on`, `eq5_freq`, `eq5_gain`, `eq5_q` | EQ 5 Bell | | On · 5 kHz · 0 dB · 1,00 |
| `eq6_on`, `eq6_freq`, `eq6_gain`, `eq6_q` | EQ 6 High | | Off · 12 kHz · 0 dB · 0,707 |
| `eq6_type` | EQ 6 Type | High Cut / High Shelf | High Cut |

INIT del EQ «sin intervenir»: las bandas 2–5 están en IN a 0 dB (identidad exacta) y
los cortes 1 y 6 fuera. La salida con EQ IN es idéntica, muestra a muestra, a la salida
con el EQ apagado (prueba `params.init_values`). `eq6_gain` se conserva al pasar de
HIGH SHELF a HIGH CUT y vuelve al regresar.

## LFO

| ID | Nombre | Rango | INIT |
|---|---|---|---|
| `lfo_on` | LFO On | Off/On | Off |
| `lfo_mode` | LFO Mode | Manual / Auto | Manual |
| `lfo_shape` | LFO Shape | Sine, Triangle, Saw, Square, Random | Sine |
| `lfo_rate` | LFO Rate | 0,01–20 Hz (log) | 1,00 Hz |
| `lfo_rate_sync` | LFO Rate Sync | Off/On | Off |
| `lfo_rate_note` | LFO Rate Note | 22 valores | 1/2 |
| `lfo_phase` | LFO Phase | 0–360° | 0° |
| `lfo_depth_1` … `lfo_depth_6` | LFO Depth 1…6 | 0–100 % | 0 % |
| `lfo_depth_pan` | LFO Depth Pan | 0–100 % | 0 % |

RATE sincronizado, de lento a rápido: 8 BAR, 4 BAR, 2 BAR, 1/1D, 1 BAR, 1/1, 1/2D, 1/1T,
1/2, 1/4D, 1/2T, 1/4, 1/8D, 1/4T, 1/8, 1/16D, 1/8T, 1/16, 1/32D, 1/16T, 1/32, 1/32T.
Los compases usan el compás del host (4/4 de reserva): 1 BAR coincide con 1/1 en 4/4 y
dura 3 negras en 3/4. El límite de 20 Hz solo afecta al mando libre (1/32T a 120 BPM = 24 Hz).

## Estado guardado

El estado (XML binario de JUCE, raíz `LABD`, `labd_state_version` = 1) guarda los 54
parámetros —incluidos los dos valores de cada SYNC (ms y figura por canal, Hz y figura
del RATE) y el MIX conservado durante WET ONLY— y la banda seleccionada en el editor
(`ui_selected_band`). Un estado ajeno o corrupto se ignora sin cambiar nada.
