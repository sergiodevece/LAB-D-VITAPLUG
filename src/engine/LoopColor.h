// VitaPlug LAB-D · motor de audio (sin JUCE)
// Color dentro del bucle de realimentación (se aplica a entrada + realimentación
// antes de escribir en la línea, así que también colorea el primer eco).
//
//  CLEAN: y = x + drive · (tanh(4x)/4 - x). Con DRIVE 0 es la identidad exacta.
//  TAPE : pasa altos 1 polo (35 Hz) → saturación asimétrica → pasa bajos 1 polo
//         (6,5 kHz bajando a 4,2 kHz con DRIVE). Saturación tanh(g·x)/g en la
//         parte positiva y tanh(1,35·g·x)/(1,35·g) en la negativa, g = 1,6 + 4,4·DRIVE.
//
// Estabilidad: ambas curvas tienen pendiente 1 en el origen y <= 1 en todo el
// rango, y los filtros de un polo tienen |H| <= 1. Con FEEDBACK <= 0,98 la
// ganancia incremental del bucle es <= 0,98: las repeticiones siempre decaen.
#pragma once

#include "engine/Smoothers.h"

#include <cmath>

namespace labd {

class LoopColor {
public:
    static constexpr float kCleanCurve = 4.0f;
    static constexpr float kTapeMinDrive = 1.6f;
    static constexpr float kTapeDriveRange = 4.4f;
    static constexpr float kTapeAsymmetry = 1.35f;
    static constexpr double kTapeHighPassHz = 35.0;
    static constexpr double kTapeLowPassHz = 6500.0;
    static constexpr double kTapeLowPassDriveDrop = 0.35;

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        hpA = (float) (1.0 - std::exp (-6.283185307179586 * kTapeHighPassHz / fs));
        reset();
    }

    void reset() noexcept { hpZ = lpZ = 0.0f; }

    // Coeficiente del pasa bajos de TAPE para un DRIVE dado (por bloque).
    float lowPassCoefficient (float drive) const noexcept
    {
        const double fc = kTapeLowPassHz * (1.0 - kTapeLowPassDriveDrop * drive);
        return (float) (1.0 - std::exp (-6.283185307179586 * fc / fs));
    }

    static float cleanShape (float x, float drive) noexcept
    {
        if (drive <= 0.0f)
            return x;
        return x + drive * (std::tanh (kCleanCurve * x) / kCleanCurve - x);
    }

    static float tapeShape (float x, float drive) noexcept
    {
        const float g = kTapeMinDrive + kTapeDriveRange * drive;
        const float gg = x >= 0.0f ? g : g * kTapeAsymmetry;
        return std::tanh (gg * x) / gg;
    }

    // tapeMix: 0 = CLEAN, 1 = TAPE (fundido al conmutar).
    float process (float x, float drive, float tapeMix, float lpA) noexcept
    {
        const float clean = cleanShape (x, drive);
        if (tapeMix <= 0.0f)
        {
            // Mantiene los filtros de TAPE en marcha para un cambio sin saltos.
            hpZ += hpA * (x - hpZ);
            lpZ += lpA * (clean - lpZ);
            return clean;
        }
        hpZ += hpA * (x - hpZ);
        const float s = tapeShape (x - hpZ, drive);
        lpZ += lpA * (s - lpZ);
        return clean + tapeMix * (lpZ - clean);
    }

private:
    double fs = 48000.0;
    float hpA = 0.0f, hpZ = 0.0f, lpZ = 0.0f;
};

} // namespace labd
