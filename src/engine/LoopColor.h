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
//
// Compensación (fuera del bucle): como la curva no puede amplificar dentro del
// bucle, el eco que se oye recibe una ganancia de compensación que mantiene su
// nivel a -12 dBFS igual que en CLEAN sin DRIVE (makeupGain). No afecta a la
// realimentación ni a la estabilidad.
#pragma once

#include "engine/Smoothers.h"

#include <algorithm>
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

    static constexpr float kMakeupReference = 0.25f; // -12 dBFS
    static constexpr int kMakeupSteps = 33;           // tabla de DRIVE 0..1

    // Ganancia del primer armónico de un seno de amplitud kMakeupReference al
    // pasar por la curva (cuadratura de 64 puntos).
    static double fundamentalGain (bool tape, float drive) noexcept
    {
        constexpr int n = 64;
        double sum = 0.0;
        for (int k = 0; k < n; ++k)
        {
            const double th = (k + 0.5) / n * 6.283185307179586;
            const float x = kMakeupReference * (float) std::sin (th);
            sum += (double) (tape ? tapeShape (x, drive) : cleanShape (x, drive)) * std::sin (th);
        }
        return 2.0 / n * sum / kMakeupReference;
    }

    struct MakeupTables
    {
        float clean[kMakeupSteps], tape[kMakeupSteps];
        MakeupTables()
        {
            for (int i = 0; i < kMakeupSteps; ++i)
            {
                const float d = (float) i / (kMakeupSteps - 1);
                clean[i] = (float) (1.0 / fundamentalGain (false, d));
                tape[i] = (float) (1.0 / fundamentalGain (true, d));
            }
            clean[0] = 1.0f;
        }
    };

    static const MakeupTables& makeupTables() noexcept
    {
        static const MakeupTables tables; // se construye en prepare(), fuera del hilo de audio
        return tables;
    }

    // Ganancia de compensación del eco audible: el primer armónico de un seno a
    // -12 dBFS sale al mismo nivel que en CLEAN sin DRIVE. 1 exacto con CLEAN y DRIVE 0.
    static float makeupGain (float drive, float tapeMix) noexcept
    {
        if (drive <= 0.0f && tapeMix <= 0.0f)
            return 1.0f;
        const auto& t = makeupTables();
        const float pos = std::clamp (drive, 0.0f, 1.0f) * (kMakeupSteps - 1);
        const int i = std::min ((int) pos, kMakeupSteps - 2);
        const float f = pos - (float) i;
        const float clean = t.clean[i] + f * (t.clean[i + 1] - t.clean[i]);
        if (tapeMix <= 0.0f)
            return clean;
        const float tape = t.tape[i] + f * (t.tape[i + 1] - t.tape[i]);
        return clean + tapeMix * (tape - clean);
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
