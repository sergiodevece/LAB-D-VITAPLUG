#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

namespace {
// Índice del primer eco en L para un ajuste (impulso centrado, 100 % wet, FB 0).
size_t firstEcho (EngineParams p, double fs, double bpm, size_t len, float* level = nullptr, bool hasBpm = true)
{
    RenderSpec spec;
    spec.fs = fs;
    spec.transport = hostAt (bpm);
    spec.transport.hasBpm = hasBpm;
    const auto out = render (p, impulse (len), spec);
    const auto peaks = findPeaks (out.L, 0.25f);
    if (peaks.empty())
        return SIZE_MAX;
    if (level != nullptr)
        *level = peaks[0].value;
    return peaks[0].index;
}
} // namespace

TEST_CASE ("delay.sync_120bpm_48k")
{
    auto p = cleanWetParams();
    p.sync = true;
    struct Row { const char* note; double ms; };
    for (const Row r : { Row { "1/1", 2000.0 }, Row { "1/4", 500.0 }, Row { "1/8D", 375.0 } })
    {
        p.noteL = tempo::noteIndex (r.note);
        float level = 0.0f;
        const size_t idx = firstEcho (p, 48000.0, 120.0, (size_t) (r.ms * 48.0) + 4800, &level);
        const double expected = r.ms * 48.0;
        CHECK_MSG (std::abs ((double) idx - expected) <= 1.0, "%s: eco en %zu, esperado %.0f", r.note, idx, expected);
        CHECK_NEAR (level, 1.0, 1e-6);
        EVIDENCE ("%s @120 BPM/48 kHz: primer eco en muestra %zu (%.3f ms), nivel %.6f", r.note, idx, (double) idx / 48.0, level);
    }
}

TEST_CASE ("delay.free_time_and_extremes")
{
    auto p = cleanWetParams();
    for (double ms : { 350.0, 1.0, 4000.0, 123.0 })
    {
        p.timeMsL = ms;
        float level = 0.0f;
        const size_t idx = firstEcho (p, 48000.0, 120.0, (size_t) (ms * 48.0) + 4800, &level);
        CHECK_MSG (std::abs ((double) idx - ms * 48.0) <= 1.0, "%.0f ms: eco en %zu", ms, idx);
        CHECK_NEAR (level, 1.0, 1e-6);
        EVIDENCE ("libre %.0f ms: eco en muestra %zu, nivel %.6f", ms, idx, level);
    }
    // Fuera de rango: se acota a 1..4000 ms.
    p.timeMsL = 9000.0;
    const size_t idx = firstEcho (p, 48000.0, 120.0, 4000 * 48 + 4800);
    CHECK_MSG (std::abs ((double) idx - 4000.0 * 48.0) <= 1.0, "acotado a 4000 ms: %zu", idx);
}

TEST_CASE ("delay.other_bpm_and_rates")
{
    auto p = cleanWetParams();
    p.sync = true;
    struct Row { double fs; double bpm; const char* note; };
    for (const Row r : { Row { 48000, 90.0, "1/4" }, Row { 48000, 140.0, "1/8T" }, Row { 44100, 120.0, "1/8D" },
                         Row { 96000, 120.0, "1/16" }, Row { 48000, 400.0, "1/32T" }, Row { 44100, 97.3, "1/4D" } })
    {
        p.noteL = tempo::noteIndex (r.note);
        const double ms = tempo::noteMs (p.noteL, r.bpm);
        const double expected = ms * r.fs / 1000.0;
        float level = 0.0f;
        const size_t idx = firstEcho (p, r.fs, r.bpm, (size_t) expected + 4096, &level);
        CHECK_MSG (std::abs ((double) idx - expected) <= 1.0, "%s @%.1f BPM %.0f Hz: %zu vs %.2f", r.note, r.bpm, r.fs, idx, expected);
        CHECK (std::abs (level) > 0.5f);
        EVIDENCE ("%s @%.1f BPM, %.1f kHz: eco en %zu (esperado %.2f), pico %.4f", r.note, r.bpm, r.fs / 1000, idx, expected, level);
    }
}

TEST_CASE ("delay.longest_figure_20bpm_18s")
{
    auto p = cleanWetParams();
    p.sync = true;
    p.noteL = tempo::noteIndex ("1/1D");
    DelayEngine e;
    RenderSpec spec;
    spec.transport = hostAt (20.0);
    const size_t len = 18 * 48000 + 4800;
    const auto out = render (p, impulse (len), spec, &e);
    const auto peaks = findPeaks (out.L, 0.25f);
    CHECK (! peaks.empty());
    if (! peaks.empty())
    {
        CHECK_MSG (peaks[0].index == 18 * 48000, "1/1D a 20 BPM en %zu", peaks[0].index);
        EVIDENCE ("1/1D a 20 BPM: eco en %zu muestras = %.3f s (búfer %zu muestras/canal)",
                  peaks[0].index, peaks[0].index / 48000.0, e.delayCapacitySamples());
    }
    for (double fs : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        DelayEngine probe;
        probe.prepare (fs, 512, 2);
        CHECK ((double) probe.delayCapacitySamples() >= 18.0 * fs + 4.0);
    }
}

TEST_CASE ("delay.missing_bpm_uses_120_then_last_valid")
{
    auto p = cleanWetParams();
    p.sync = true;
    p.noteL = tempo::noteIndex ("1/4");
    // Sin BPM desde el inicio: reserva 120 → 500 ms.
    size_t idx = firstEcho (p, 48000.0, 0.0, 30000, nullptr, false);
    CHECK_MSG (idx == 24000, "sin BPM: %zu", idx);
    EVIDENCE ("sin BPM del host: 1/4 → muestra %zu (120 BPM de reserva)", idx);

    // BPM 100 válido y después ausente: conserva 100 → 600 ms.
    DelayEngine e;
    e.prepare (48000.0, 512, 2);
    e.setParams (p);
    Stereo s (48000);
    float* ch[2] = { s.L.data(), s.R.data() };
    e.process (ch, 2, 512, hostAt (100.0));
    TransportInfo none;
    Stereo imp = impulse (40000);
    for (size_t pos = 0; pos < imp.size(); pos += 512)
    {
        float* c[2] = { imp.L.data() + pos, imp.R.data() + pos };
        e.process (c, 2, (int) std::min<size_t> (512, imp.size() - pos), none);
    }
    const auto peaks = findPeaks (imp.L, 0.25f);
    CHECK (! peaks.empty() && peaks[0].index == 28800);
    CHECK_NEAR (e.telemetry().bpm, 100.0, 0.0);
    CHECK (! e.telemetry().bpmFromHost);
    EVIDENCE ("BPM 100 y luego ausente: eco en %zu (600 ms), telemetría %.1f BPM",
              peaks.empty() ? 0 : peaks[0].index, e.telemetry().bpm);
}

TEST_CASE ("delay.sync_mode_memory_and_link")
{
    auto p = cleanWetParams();
    p.timeMsL = 350.0;
    p.timeMsR = 500.0;
    p.noteL = tempo::noteIndex ("1/8D");
    p.noteR = tempo::noteIndex ("1/4");
    double l = 0, r = 0;
    DelayEngine::effectiveTimesMs (p, 120.0, l, r);
    CHECK_NEAR (l, 350.0, 0.0);
    CHECK_NEAR (r, 500.0, 0.0);
    p.sync = true;
    DelayEngine::effectiveTimesMs (p, 120.0, l, r);
    CHECK_NEAR (l, 375.0, 1e-9);
    CHECK_NEAR (r, 500.0, 1e-9);
    p.link = true;
    DelayEngine::effectiveTimesMs (p, 120.0, l, r);
    CHECK_NEAR (l, 375.0, 1e-9);
    CHECK_NEAR (r, 375.0, 1e-9);
    p.sync = false;
    DelayEngine::effectiveTimesMs (p, 120.0, l, r);
    CHECK_NEAR (r, 350.0, 0.0);
    p.link = false;
    DelayEngine::effectiveTimesMs (p, 120.0, l, r);
    CHECK_NEAR (r, 500.0, 0.0); // los valores guardados de R se conservan
    EVIDENCE ("LINK parte de L (350 ms / 1/8D); al soltarlo R vuelve a 500 ms / 1/4");

    // Medido con impulsos en R.
    p.link = true;
    RenderSpec spec;
    const auto out = render (p, impulse (24000), spec);
    const auto pk = findPeaks (out.R, 0.25f);
    CHECK (! pk.empty() && pk[0].index == 16800);
}

TEST_CASE ("delay.tempo_change_smooth")
{
    auto p = cleanWetParams();
    p.sync = true;
    p.noteL = p.noteR = tempo::noteIndex ("1/4");
    p.feedback = 0.6f;
    p.wetOnly = false;
    p.mix = 0.5f;
    const double fs = 48000.0;
    const size_t n = (size_t) (fs * 6);
    const auto in = tone (n, 220.0, fs, 0.5f);
    RenderSpec spec;
    spec.hook = [&] (DelayEngine&, EngineParams&, TransportInfo& t, size_t pos) {
        if (pos >= (size_t) (fs * 2) && pos < (size_t) (fs * 4))
            t.bpm = 120.0 - 60.0 * (double) (pos - (size_t) (fs * 2)) / (fs * 2); // rampa 120 → 60
        else if (pos >= (size_t) (fs * 4))
            t.bpm = 60.0;
    };
    const auto out = render (p, in, spec);
    CHECK (allFinite (out));
    const float natural = (float) (2.0 * kPi * 220.0 / fs) * 0.5f * 1.6f; // dry + wet acotado
    const float worst = maxStep (out.L);
    CHECK_MSG (worst < natural * 2.0f, "salto máximo %.4f (referencia %.4f)", worst, natural);
    EVIDENCE ("rampa 120→60 BPM con FB 60 %%: salto máximo %.4f, referencia de un tono limpio %.4f", worst, natural);

    // Tras el cambio, 1/4 a 60 BPM = 1000 ms.
    auto p2 = cleanWetParams();
    p2.sync = true;
    p2.noteL = tempo::noteIndex ("1/4");
    DelayEngine e;
    e.prepare (fs, 512, 2);
    e.setParams (p2);
    Stereo warm (48000);
    for (size_t pos = 0; pos < warm.size(); pos += 512)
    {
        float* c[2] = { warm.L.data() + pos, warm.R.data() + pos };
        e.process (c, 2, 512, hostAt (pos < 24000 ? 120.0 : 60.0));
    }
    auto imp = impulse (60000);
    for (size_t pos = 0; pos < imp.size(); pos += 512)
    {
        float* c[2] = { imp.L.data() + pos, imp.R.data() + pos };
        e.process (c, 2, (int) std::min<size_t> (512, imp.size() - pos), hostAt (60.0));
    }
    const auto pk = findPeaks (imp.L, 0.25f);
    CHECK (! pk.empty() && pk[0].index == 48000);
    EVIDENCE ("tras pasar a 60 BPM: 1/4 → eco en %zu (1000 ms)", pk.empty() ? 0 : pk[0].index);
}

TEST_CASE ("delay.fractional_time_interpolated")
{
    // 350,01 ms a 48 kHz = 16800,48 muestras: el eco se reparte entre dos muestras.
    auto p = cleanWetParams();
    p.timeMsL = 350.01;
    const auto out = render (p, impulse (20000));
    const auto pk = findPeaks (out.L, 0.2f);
    CHECK (! pk.empty() && (pk[0].index == 16800 || pk[0].index == 16801));
    // Un tono pasa sin pérdida apreciable (Hermite).
    const auto t = render (p, tone (48000, 1000.0, 48000.0, 0.5f));
    const double a = toneAmplitude (t.L, 24000, 48000, 1000.0, 48000.0);
    CHECK_NEAR (a, 0.5, 0.005);
    EVIDENCE ("350,01 ms: pico en %zu; tono 1 kHz retardado con amplitud %.5f (entrada 0,5)", pk.empty() ? 0 : pk[0].index, a);
}

TEST_CASE ("delay.time_jump_crossfade_no_click")
{
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 300.0;
    p.feedback = 0.5f;
    const double fs = 48000.0;
    const auto in = tone ((size_t) (fs * 4), 440.0, fs, 0.5f);
    const auto ref = render (p, in);
    RenderSpec spec;
    spec.hook = [&] (DelayEngine&, EngineParams& q, TransportInfo&, size_t pos) {
        // Saltos de tiempo grandes cada 0,25 s, también SYNC y LINK.
        const int step = (int) (pos / 12000);
        q.timeMsL = (step % 3 == 0) ? 300.0 : (step % 3 == 1 ? 1200.0 : 37.0);
        q.timeMsR = (step % 2 == 0) ? 450.0 : 90.0;
        q.sync = (step % 5) == 4;
        q.link = (step % 7) == 6;
    };
    const auto out = render (p, in, spec);
    const float refStep = maxStep (ref.L, 24000);
    const float worst = std::max (maxStep (out.L, 24000), maxStep (out.R, 24000));
    CHECK (allFinite (out));
    CHECK_MSG (worst <= refStep * 2.5f, "salto %.4f vs referencia %.4f", worst, refStep);
    EVIDENCE ("saltos de TIME/SYNC/LINK cada 250 ms: peor salto %.4f, referencia estática %.4f (x%.2f)",
              worst, refStep, worst / refStep);
}
