// Utilidades de render y medida para las pruebas del motor.
// Señales generadas localmente: impulsos, tonos y ruido con semilla fija.
#pragma once

#include "engine/DelayEngine.h"
#include "TestFramework.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

namespace labdtest {

constexpr double kPi = 3.14159265358979323846;

struct Stereo
{
    std::vector<float> L, R;
    explicit Stereo (size_t n = 0) : L (n, 0.0f), R (n, 0.0f) {}
    size_t size() const { return L.size(); }
};

// Parámetros de referencia para medir el retardo: limpio, sin EQ/LFO, 100 % wet.
inline labd::EngineParams cleanWetParams()
{
    labd::EngineParams p;
    p.feedback = 0.0f;
    p.mix = 1.0f;
    p.wetOnly = true;
    p.eq.eqOn = false;
    p.lfoOn = false;
    return p;
}

inline labd::TransportInfo hostAt (double bpm, bool playing = true, double ppq = 0.0)
{
    labd::TransportInfo t;
    t.hasBpm = true;
    t.bpm = bpm;
    t.isPlaying = playing;
    t.hasPpq = true;
    t.ppq = ppq;
    t.hasTimeSig = true;
    return t;
}

using BlockHook = std::function<void (labd::DelayEngine&, labd::EngineParams&, labd::TransportInfo&, size_t start)>;

struct RenderSpec
{
    double fs = 48000.0;
    int block = 512;
    int channels = 2;
    labd::TransportInfo transport = hostAt (120.0);
    BlockHook hook; // se llama antes de cada bloque (automatización)
    bool advancePpq = true;
};

// Procesa 'input' (copiado) y devuelve la salida.
inline Stereo render (const labd::EngineParams& params, const Stereo& input, const RenderSpec& spec = {},
                      labd::DelayEngine* engineOut = nullptr)
{
    labd::DelayEngine local;
    labd::DelayEngine& e = engineOut != nullptr ? *engineOut : local;
    e.prepare (spec.fs, spec.block, spec.channels);
    labd::EngineParams p = params;
    labd::TransportInfo t = spec.transport;
    Stereo out = input;
    const size_t n = input.size();
    for (size_t pos = 0; pos < n; pos += (size_t) spec.block)
    {
        const int len = (int) std::min ((size_t) spec.block, n - pos);
        if (spec.hook)
            spec.hook (e, p, t, pos);
        e.setParams (p);
        float* ch[2] = { out.L.data() + pos, out.R.data() + pos };
        e.process (ch, spec.channels, len, t);
        if (spec.advancePpq && t.hasBpm && t.isPlaying)
            t.ppq += (double) len / spec.fs * t.bpm / 60.0;
    }
    if (spec.channels == 1)
        std::fill (out.R.begin(), out.R.end(), 0.0f);
    return out;
}

inline Stereo impulse (size_t n, float l = 1.0f, float r = 1.0f, size_t at = 0)
{
    Stereo s (n);
    s.L[at] = l;
    s.R[at] = r;
    return s;
}

inline Stereo tone (size_t n, double freq, double fs, float amp, size_t start = 0, size_t stop = SIZE_MAX)
{
    Stereo s (n);
    for (size_t i = start; i < std::min (n, stop); ++i)
        s.L[i] = s.R[i] = amp * (float) std::sin (2.0 * kPi * freq * (double) (i - start) / fs);
    return s;
}

inline Stereo noise (size_t n, float amp, std::uint32_t seed = 1234, size_t stop = SIZE_MAX)
{
    Stereo s (n);
    std::uint32_t x = seed;
    auto rnd = [&x] { x ^= x << 13; x ^= x >> 17; x ^= x << 5; return (float) ((double) x / 4294967295.0 * 2.0 - 1.0); };
    for (size_t i = 0; i < std::min (n, stop); ++i)
    {
        s.L[i] = amp * rnd();
        s.R[i] = amp * rnd();
    }
    return s;
}

struct Peak
{
    size_t index;
    float value;
};

// Picos locales por encima de un umbral, separados al menos minGap muestras.
inline std::vector<Peak> findPeaks (const std::vector<float>& x, float threshold, size_t minGap = 64)
{
    std::vector<Peak> peaks;
    size_t i = 0;
    while (i < x.size())
    {
        if (std::abs (x[i]) >= threshold)
        {
            size_t best = i;
            const size_t end = std::min (x.size(), i + minGap);
            for (size_t j = i; j < end; ++j)
                if (std::abs (x[j]) > std::abs (x[best]))
                    best = j;
            peaks.push_back ({ best, x[best] });
            i = end;
        }
        else
            ++i;
    }
    return peaks;
}

// Energía en una ventana [a, b).
inline double energy (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0.0;
    for (size_t i = a; i < std::min (b, x.size()); ++i)
        e += (double) x[i] * x[i];
    return e;
}

inline double rms (const std::vector<float>& x, size_t a, size_t b)
{
    const size_t e = std::min (b, x.size());
    return e > a ? std::sqrt (energy (x, a, e) / (double) (e - a)) : 0.0;
}

// Amplitud de una componente sinusoidal (Goertzel normalizado) en [a, b).
inline double toneAmplitude (const std::vector<float>& x, size_t a, size_t b, double freq, double fs)
{
    double re = 0.0, im = 0.0;
    const size_t e = std::min (b, x.size());
    for (size_t i = a; i < e; ++i)
    {
        const double ph = 2.0 * kPi * freq * (double) i / fs;
        re += x[i] * std::cos (ph);
        im += x[i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / (double) (e - a);
}

inline float maxAbs (const std::vector<float>& x, size_t a = 0, size_t b = SIZE_MAX)
{
    float m = 0.0f;
    for (size_t i = a; i < std::min (b, x.size()); ++i)
        m = std::max (m, std::abs (x[i]));
    return m;
}

inline bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i)
        if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i]))
            return false;
    return true;
}

// Mayor salto entre muestras consecutivas en [a, b).
inline float maxStep (const std::vector<float>& x, size_t a = 1, size_t b = SIZE_MAX)
{
    float m = 0.0f;
    for (size_t i = std::max<size_t> (a, 1); i < std::min (b, x.size()); ++i)
        m = std::max (m, std::abs (x[i] - x[i - 1]));
    return m;
}

inline double db (double g) { return 20.0 * std::log10 (std::max (g, 1.0e-12)); }

} // namespace labdtest
