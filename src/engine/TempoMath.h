// VitaPlug LAB-D · motor de audio (sin JUCE)
// Figuras musicales, tempo y compás.
//
// Una negra dura 60000 / BPM ms. 1/N dura 4/N negras. Puntillo x 3/2, tresillo x 2/3.
#pragma once

namespace labd::tempo {

constexpr double kMinBpm = 20.0;      // rango musical declarado
constexpr double kMaxBpm = 400.0;
constexpr double kDefaultBpm = 120.0; // reserva hasta recibir un BPM válido

constexpr double kMinTimeMs = 1.0;
constexpr double kMaxFreeTimeMs = 4000.0;
// 1/1D a 20 BPM = 6 negras x 3000 ms = 18000 ms.
constexpr double kMaxSyncTimeMs = 6.0 * 60000.0 / kMinBpm;
constexpr double kMaxDelayMs = kMaxSyncTimeMs;

struct Note
{
    const char* label;
    double quarters; // duración en negras
};

// Figuras de TIME, de menor a mayor duración (orden del mando).
constexpr int kNumNotes = 18;
extern const Note kNotes[kNumNotes];

// RATE del LFO: ciclos en compases y figuras, de lento a rápido (en 4/4;
// 1 BAR coincide con 1/1 en 4/4 y es más corto en 3/4).
struct RateChoice
{
    const char* label;
    double quarters; // duración del ciclo en negras (si bars == 0)
    int bars;        // > 0: ciclo de N compases, depende del compás del host
};
constexpr int kNumRates = 4 + kNumNotes;
extern const RateChoice kRates[kNumRates];

int noteIndex (const char* label) noexcept;  // -1 si no existe
int rateIndex (const char* label) noexcept;  // -1 si no existe

inline double quarterMs (double bpm) noexcept { return 60000.0 / bpm; }
double noteMs (int noteIdx, double bpm) noexcept;
double barQuarters (int sigNumerator, int sigDenominator) noexcept;
double rateCycleQuarters (int rateIdx, int sigNumerator, int sigDenominator) noexcept;
double rateHz (int rateIdx, double bpm, int sigNumerator, int sigDenominator) noexcept;

// Devuelve el BPM utilizable (acotado al rango) o -1 si el dato no es válido.
double sanitiseBpm (double bpm) noexcept;

} // namespace labd::tempo
