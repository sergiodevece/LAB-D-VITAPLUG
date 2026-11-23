// VitaPlug LAB-D · motor de audio (sin JUCE)
// Oscilador de baja frecuencia con fase acumulada en ciclos (double).
// RND transita entre valores pseudoaleatorios que dependen solo del índice de
// ciclo y de la semilla: un render offline es reproducible.
#pragma once

#include <cmath>
#include <cstdint>

namespace labd {

enum class LfoShape { Sine = 0, Triangle, Saw, Square, Random };

constexpr std::uint32_t kLfoDefaultSeed = 0x4C414244u; // "LABD"

// Valor pseudoaleatorio en [-1, 1] para un ciclo.
inline float lfoRandomLevel (std::int64_t cycleIndex, std::uint32_t seed) noexcept
{
    std::uint64_t x = (std::uint64_t) cycleIndex * 0x9E3779B97F4A7C15ull + seed;
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27; x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return (float) ((double) (x >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0);
}

// Forma de onda en [-1, 1] para una posición absoluta en ciclos.
inline float lfoWave (LfoShape shape, double cyclePosition, std::uint32_t seed) noexcept
{
    const double floorPos = std::floor (cyclePosition);
    const double p = cyclePosition - floorPos;
    switch (shape)
    {
        case LfoShape::Sine:     return (float) std::sin (6.283185307179586 * p);
        case LfoShape::Triangle: { const double q = p + 0.25 - std::floor (p + 0.25); return (float) (1.0 - 4.0 * std::abs (q - 0.5)); }
        case LfoShape::Saw:      return (float) (2.0 * p - 1.0);
        case LfoShape::Square:   return p < 0.5 ? 1.0f : -1.0f;
        case LfoShape::Random:
        {
            const auto n = (std::int64_t) floorPos;
            const float a = lfoRandomLevel (n, seed), b = lfoRandomLevel (n + 1, seed);
            const float w = (float) (0.5 - 0.5 * std::cos (3.141592653589793 * p));
            return a + (b - a) * w;
        }
    }
    return 0.0f;
}

class Lfo {
public:
    void reset (double cyclePosition = 0.0) noexcept { position = cyclePosition; }
    void advance (double cycles) noexcept { position += cycles; }
    void syncTo (double cyclePosition) noexcept { position = cyclePosition; }
    double getPosition() const noexcept { return position; }

    float value (LfoShape shape, double phaseOffsetCycles, std::uint32_t seed = kLfoDefaultSeed) const noexcept
    {
        return lfoWave (shape, position + phaseOffsetCycles, seed);
    }

    double phase (double phaseOffsetCycles) const noexcept
    {
        const double x = position + phaseOffsetCycles;
        return x - std::floor (x);
    }

private:
    double position = 0.0;
};

} // namespace labd
