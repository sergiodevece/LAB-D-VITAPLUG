#include "engine/Svf.h"

#include <algorithm>
#include <cmath>

namespace labd {

namespace {
constexpr double kPi = 3.14159265358979323846;

void finish (SvfCoeffs& c) noexcept
{
    c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
    c.a2 = c.g * c.a1;
    c.a3 = c.g * c.a2;
}
} // namespace

double clampEqFreq (double freqHz, double sampleRate) noexcept
{
    const double top = std::min (kMaxEqFreq, kNyquistMargin * sampleRate);
    if (! std::isfinite (freqHz))
        freqHz = 1000.0;
    return std::clamp (freqHz, kMinEqFreq, top);
}

SvfCoeffs designSvf (FilterType type, double freqHz, double gainDb, double q, double sampleRate) noexcept
{
    SvfCoeffs c;
    const double f = clampEqFreq (freqHz, sampleRate);
    q = std::clamp (q, 0.1, 20.0);
    const double w = std::tan (kPi * f / sampleRate);
    const double A = std::pow (10.0, gainDb / 40.0);

    switch (type)
    {
        case FilterType::LowCut:
            c.g = w; c.k = 1.0 / q;
            c.m0 = 1.0; c.m1 = -c.k; c.m2 = -1.0;
            break;
        case FilterType::HighCut:
            c.g = w; c.k = 1.0 / q;
            c.m0 = 0.0; c.m1 = 0.0; c.m2 = 1.0;
            break;
        case FilterType::Bell:
            c.g = w; c.k = 1.0 / (q * A);
            c.m0 = 1.0; c.m1 = c.k * (A * A - 1.0); c.m2 = 0.0;
            break;
        case FilterType::LowShelf:
            c.g = w / std::sqrt (A); c.k = 1.0 / kShelfQ;
            c.m0 = 1.0; c.m1 = c.k * (A - 1.0); c.m2 = A * A - 1.0;
            break;
        case FilterType::HighShelf:
            c.g = w * std::sqrt (A); c.k = 1.0 / kShelfQ;
            c.m0 = A * A; c.m1 = c.k * (1.0 - A) * A; c.m2 = 1.0 - A * A;
            break;
    }
    finish (c);
    return c;
}

std::complex<double> svfResponse (const SvfCoeffs& c, double freqHz, double sampleRate) noexcept
{
    // Prototipo analógico con s = j · tan(pi f / fs) / g (transformación bilineal
    // con prewarp, idéntica a la integración trapezoidal del filtro).
    const double wf = std::tan (kPi * std::min (freqHz, 0.4999 * sampleRate) / sampleRate);
    const std::complex<double> s (0.0, wf / c.g);
    const std::complex<double> den = s * s + c.k * s + 1.0;
    return c.m0 + (c.m1 * s + c.m2) / den;
}

} // namespace labd
