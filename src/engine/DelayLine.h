// VitaPlug LAB-D · motor de audio (sin JUCE)
// Línea de retardo circular con lectura Hermite (Catmull-Rom) y un lector de
// tiempo que cambia de posición con un fundido entre dos lecturas.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace labd {

class DelayLine {
public:
    // Reserva memoria (fuera del callback). maxDelaySamples incluye margen.
    void prepare (int maxDelaySamples)
    {
        size_t size = 1;
        while (size < (size_t) maxDelaySamples + 8)
            size <<= 1;
        buffer.assign (size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
        valid = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        valid = 0;
        ghostValid = 0;
    }

    // Vaciado lógico en O(1), apto para el hilo de audio: las muestras escritas
    // antes de esta llamada se leen como silencio con read(). Siguen accesibles
    // con readOld() (la "cola fantasma") para poder fundirlas sin clic.
    void forget() noexcept
    {
        ghostValid = valid;
        valid = 0;
    }

    void push (float x) noexcept
    {
        buffer[writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
        if (valid < mask)
            ++valid;
    }

    // Lectura antes de escribir la muestra actual: read(N) devuelve x[n - N]
    // para N entero (exacto). delaySamples >= 2.
    float read (double delaySamples) const noexcept { return readImpl<false> (delaySamples); }

    // Solo las muestras anteriores al último forget(). read() + readOld() es la
    // lectura completa del búfer: en el instante del reinicio no hay salto.
    float readOld (double delaySamples) const noexcept { return readImpl<true> (delaySamples); }

    size_t capacity() const noexcept { return buffer.size(); }

private:
    template <bool old>
    float at (size_t samplesAgo) const noexcept
    {
        const bool ok = old ? (samplesAgo > valid && samplesAgo - valid <= ghostValid) : samplesAgo <= valid;
        return ok ? buffer[(writeIndex - samplesAgo) & mask] : 0.0f;
    }

    template <bool old>
    float readImpl (double delaySamples) const noexcept
    {
        const auto i = (size_t) delaySamples;
        const float t = (float) (delaySamples - (double) i);
        const float x0 = at<old> (i);
        if (t == 0.0f)
            return x0;
        const float xm1 = at<old> (i - 1);
        const float x1 = at<old> (i + 1);
        const float x2 = at<old> (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    std::vector<float> buffer;
    size_t mask = 0, writeIndex = 0, valid = 0, ghostValid = 0;
};

// Lector con transición: al cambiar el tiempo funde la lectura antigua con la
// nueva (coseno elevado, suma de ganancias = 1). Si el objetivo cambia durante
// un fundido, el siguiente empieza cuando termina el actual (sin saltos).
class CrossfadeReader {
public:
    void prepare (double sampleRate, double fadeSeconds) noexcept
    {
        fadeIncrement = (float) (1.0 / std::max (1.0, sampleRate * fadeSeconds));
    }

    void reset (double delaySamples) noexcept
    {
        current = next = pending = delaySamples;
        fade = 0.0f;
        fading = false;
    }

    void setTarget (double delaySamples) noexcept { pending = delaySamples; }

    // ghost != nullptr: devuelve también la lectura de la cola anterior al
    // último forget() con las mismas posiciones y pesos.
    float process (const DelayLine& line, float* ghost = nullptr) noexcept
    {
        if (! fading && pending != current)
        {
            if (std::abs (pending - current) < 1.0e-6)
                current = pending;
            else
            {
                next = pending;
                fading = true;
                fade = 0.0f;
            }
        }

        if (! fading)
        {
            if (ghost != nullptr)
                *ghost = line.readOld (current);
            return line.read (current);
        }

        const float w = 0.5f - 0.5f * std::cos (3.14159265358979f * fade);
        const float y = line.read (current) * (1.0f - w) + line.read (next) * w;
        if (ghost != nullptr)
            *ghost = line.readOld (current) * (1.0f - w) + line.readOld (next) * w;
        fade += fadeIncrement;
        if (fade >= 1.0f)
        {
            current = next;
            fading = false;
        }
        return y;
    }

    double getCurrent() const noexcept { return fading ? next : current; }
    bool isFading() const noexcept { return fading; }

private:
    double current = 2.0, next = 2.0, pending = 2.0;
    float fade = 0.0f, fadeIncrement = 0.001f;
    bool fading = false;
};

} // namespace labd
