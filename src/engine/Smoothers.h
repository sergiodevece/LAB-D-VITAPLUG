// VitaPlug LAB-D · motor de audio (sin JUCE)
// Suavizadores deterministas usados por el motor. Sin reservas de memoria.
#pragma once

#include <algorithm>
#include <cmath>

namespace labd {

// Rampa lineal con llegada exacta al objetivo en un número fijo de muestras.
class LinearSmoother {
public:
    void prepare (double sampleRate, double rampSeconds) noexcept
    {
        rampSamples = std::max (1, (int) std::lround (sampleRate * rampSeconds));
    }

    void reset (float value) noexcept
    {
        current = target = value;
        remaining = 0;
        step = 0.0f;
    }

    void setTarget (float newTarget) noexcept
    {
        if (newTarget == target)
            return;
        target = newTarget;
        remaining = rampSamples;
        step = (target - current) / (float) remaining;
    }

    float next() noexcept
    {
        if (remaining > 0)
        {
            --remaining;
            current = (remaining == 0) ? target : current + step;
        }
        return current;
    }

    // Avanza n muestras de golpe y devuelve el valor final.
    float skip (int n) noexcept
    {
        if (remaining <= 0 || n <= 0)
            return current;
        if (n >= remaining)
        {
            remaining = 0;
            current = target;
        }
        else
        {
            remaining -= n;
            current += step * (float) n;
        }
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int remaining = 0, rampSamples = 1;
};

// Coeficiente de un polo para una constante de tiempo (tau) a una tasa dada.
inline double onePoleCoefficient (double tauSeconds, double rate) noexcept
{
    if (tauSeconds <= 0.0 || rate <= 0.0)
        return 1.0;
    return 1.0 - std::exp (-1.0 / (tauSeconds * rate));
}

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

} // namespace labd
