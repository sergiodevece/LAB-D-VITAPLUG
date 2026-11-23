// VitaPlug LAB-D · motor de audio (sin JUCE)
// ECHO EQ: seis bandas a la salida del bucle (solo afecta al wet).
//   1 LOW CUT (12 dB/oct, FREQ, Q) · 2 LOW SHELF (FREQ, GAIN) · 3-5 BELL
//   6 HIGH CUT (12 dB/oct, FREQ, Q) o HIGH SHELF (FREQ, GAIN), conmutable.
#pragma once

#include "engine/Svf.h"

#include <array>
#include <cmath>

namespace labd {

constexpr int kNumEqBands = 6;
constexpr int kMaxEqBlock = 64; // tamaño máximo de sub-bloque admitido por process()

struct EqBandParams
{
    bool on = true;
    double freq = 1000.0;
    double gainDb = 0.0;
    double q = 0.7071;
    bool highShelf = false; // solo banda 6: false = HIGH CUT, true = HIGH SHELF
};

struct EqParams
{
    bool eqOn = true;
    std::array<EqBandParams, kNumEqBands> band {};
};

// Tipo de filtro de una banda (índice 0..5).
FilterType bandFilterType (int bandIndex, bool highShelf) noexcept;

class EchoEq {
public:
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;
    void setParams (const EqParams& p) noexcept { params = p; }

    // Actualiza suavizados y coeficientes para el siguiente sub-bloque.
    // freqMultipliers: factor de modulación (LFO) de cada banda, 1 = neutro.
    void updateBlock (const double* freqMultipliers, int numSamples) noexcept;

    // Procesa un sub-bloque (n <= kMaxEqBlock) en sitio. right puede ser nullptr en mono.
    void process (float* left, float* right, int numSamples) noexcept;

    // Telemetría para el editor y las pruebas.
    double baseFrequency (int b) const noexcept { return std::exp (smoothedFreq[(size_t) b]); }
    double smoothedGainDb (int b) const noexcept { return smoothedGain[(size_t) b]; }
    double smoothedQ (int b) const noexcept { return std::exp (smoothedQLog[(size_t) b]); }
    double effectiveFrequency (int b) const noexcept { return effectiveFreq[(size_t) b]; }
    const SvfCoeffs& coefficients (int b) const noexcept { return coeffs[(size_t) b]; }
    float bandAmount (int b) const noexcept { return amountNow[(size_t) b]; }
    float globalAmount() const noexcept { return globalNow; }
    FilterType activeType (int b) const noexcept { return types[(size_t) b]; }
    double sampleRate() const noexcept { return fs; }

private:
    double fs = 48000.0;
    int channels = 2;
    EqParams params {};
    bool first = true;

    std::array<double, kNumEqBands> smoothedFreq {}, smoothedGain {}, smoothedQLog {}, effectiveFreq {};
    std::array<SvfCoeffs, kNumEqBands> coeffs {};
    std::array<FilterType, kNumEqBands> types {};
    std::array<float, kNumEqBands> amountPrev {}, amountNow {};
    float globalPrev = 1.0f, globalNow = 1.0f;
    std::array<std::array<SvfState, kNumEqBands>, 2> state {};
};

} // namespace labd
