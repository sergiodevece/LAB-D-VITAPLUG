# LAB-D · Ficha del ajuste de la maqueta

Ajuste de demostración que se ve en `docs/reference/LAB-D_Frontal.png` y en
`LAB-D_Interfaz.html`. Es reproducible: `tools/UiSnapshot.cpp` lo aplica
(`applyMockupSetting`) y genera `docs/evidence/ui/editor_maqueta_1320.png`.

| Sección | Control | Valor |
|---|---|---|
| Host | Tempo / compás | 120 BPM · 4/4 |
| DELAY | SYNC / LINK | On / Off |
| | TIME L / TIME R | 1/8D (375 ms) / 1/4 (500 ms) |
| | FEEDBACK / MIX / OUTPUT | 42 % / 32 % / 0,0 dB |
| | Modo / color | STEREO / CLEAN |
| | DRIVE / DUCK | 0 % / 0 % |
| ECHO EQ | EQ IN | On, banda 3 seleccionada |
| | 1 LOW CUT | 85 Hz · Q 0,71 · IN |
| | 2 LOW SHELF | 225 Hz · −1,5 dB · IN |
| | 3 BELL | 640 Hz · −3,5 dB · Q 1,10 · IN |
| | 4 BELL | 1,5 kHz · +3,0 dB · Q 0,95 · IN |
| | 5 BELL | 3,75 kHz · −1,8 dB · Q 1,20 · IN |
| | 6 HIGH CUT | 8,1 kHz · Q 0,71 · IN |
| LFO | ON / modo / forma | On / MANUAL / SIN |
| | RATE | SYNC 1/2 (1 Hz a 120 BPM) |
| | PHASE | 0° |
| | DEPTH 1–6 / PAN | 0 · 0 · 44 · 0 · 0 · 26 % / 38 % |
| Pie | BYPASS / WET ONLY | Off / Off |

Para cargarlo a mano en un DAW: partir de INIT y ajustar los valores de la tabla. Los
números de la maqueta son una demostración, no el INIT (ver `docs/PARAMETROS.md`).
