// VitaPlug LAB-D · motor de audio (sin JUCE)
// Filtro de estado variable TPT (trapezoidal, topología de Simper). Es estable
// con coeficientes que cambian en el tiempo, por eso admite automatización y
// modulación por LFO en todo el rango. La respuesta en frecuencia se calcula a
// partir de los mismos coeficientes que procesan el audio.
#pragma once

#include <complex>

namespace labd {

enum class FilterType { LowCut, LowShelf, Bell, HighCut, HighShelf };

struct SvfCoeffs
{
    double g = 0.0, k = 1.0;
    double a1 = 1.0, a2 = 0.0, a3 = 0.0;
    double m0 = 1.0, m1 = 0.0, m2 = 0.0;
};

constexpr double kMinEqFreq = 20.0;
constexpr double kMaxEqFreq = 20000.0;
constexpr double kNyquistMargin = 0.45; // la frecuencia nunca supera 0,45 · fs
constexpr double kShelfQ = 0.70710678118654752; // pendiente fija de los shelves

// Acota la frecuencia a [20 Hz, min(20 kHz, 0,45 · fs)].
double clampEqFreq (double freqHz, double sampleRate) noexcept;

SvfCoeffs designSvf (FilterType type, double freqHz, double gainDb, double q, double sampleRate) noexcept;

// Respuesta compleja exacta de la sección digital con esos coeficientes.
std::complex<double> svfResponse (const SvfCoeffs& c, double freqHz, double sampleRate) noexcept;

struct SvfState
{
    double ic1 = 0.0, ic2 = 0.0;

    void reset() noexcept { ic1 = ic2 = 0.0; }

    double process (const SvfCoeffs& c, double v0) noexcept
    {
        const double v3 = v0 - ic2;
        const double v1 = c.a1 * ic1 + c.a2 * v3;
        const double v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
    }
};

} // namespace labd
