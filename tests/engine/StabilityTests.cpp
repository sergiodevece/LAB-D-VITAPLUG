#include "engine/AllocationGuard.h"
#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

namespace {
EngineParams busyParams()
{
    EngineParams p;
    p.feedback = 0.7f;
    p.mix = 0.5f;
    p.timeMsL = 210.0;
    p.timeMsR = 330.0;
    p.tape = true;
    p.drive = 0.4f;
    p.duck = 0.3f;
    p.eq.eqOn = true;
    const double f[6] = { 80, 200, 640, 1500, 3750, 8100 };
    for (int b = 0; b < 6; ++b)
    {
        p.eq.band[(size_t) b].on = true;
        p.eq.band[(size_t) b].freq = f[b];
        p.eq.band[(size_t) b].gainDb = (b % 2 ? 4.0 : -3.0);
        p.eq.band[(size_t) b].q = 1.2;
    }
    p.lfoOn = true;
    p.shape = LfoShape::Triangle;
    p.rateHz = 1.3;
    for (auto& d : p.depth)
        d = 0.5f;
    return p;
}
} // namespace

TEST_CASE ("stability.silence_and_finite")
{
    auto p = busyParams();
    const auto out = render (p, Stereo (48000 * 2));
    CHECK (maxAbs (out.L) == 0.0f && maxAbs (out.R) == 0.0f);
    // Ruido fuerte con FEEDBACK 98 %, TAPE DRIVE 100 y tiempo corto: acotado y decae.
    p.feedback = 0.98f;
    p.drive = 1.0f;
    p.timeMsL = p.timeMsR = 10.0;
    p.wetOnly = true;
    const double fs = 48000.0;
    const auto in = noise ((size_t) (fs * 9), 1.0f, 7, (size_t) fs);
    const auto o = render (p, in);
    CHECK (allFinite (o));
    const float peak = maxAbs (o.L);
    const double late = rms (o.L, (size_t) (fs * 8), (size_t) (fs * 9));
    CHECK (peak < 4.0f);
    CHECK (db (late) < -60.0);
    EVIDENCE ("silencio → salida 0 exacta · ruido 0 dBFS, FB 98 %%, TAPE DRIVE 100: pico %.3f, RMS a 7-8 s tras la fuente %.1f dBFS", peak, db (late));
}

TEST_CASE ("stability.clean_fb98_long_tail_decays")
{
    auto p = cleanWetParams();
    p.feedback = 0.98f;
    p.timeMsL = p.timeMsR = 10.0;
    const double fs = 48000.0;
    const auto out = render (p, noise ((size_t) (fs * 6), 0.5f, 3, 4800));
    CHECK (allFinite (out));
    const double late = rms (out.L, (size_t) (fs * 5), (size_t) (fs * 6));
    CHECK (db (late) < -60.0);
    EVIDENCE ("CLEAN FB 98 %%: RMS a 5-6 s %.1f dBFS", db (late));
}

namespace {
void automate (EngineParams& q, TransportInfo& t, size_t pos)
{
    const double s = (double) pos / 48000.0;
    q.timeMsL = 50.0 + 900.0 * (0.5 + 0.5 * std::sin (s * 1.7));
    q.timeMsR = (int (s * 3) % 2) ? 120.0 : 700.0;
    q.sync = int (s * 1.3) % 3 == 2;
    q.link = int (s * 2.1) % 4 == 3;
    q.pingPong = int (s * 2.7) % 2 == 1;
    q.tape = int (s * 1.9) % 2 == 0;
    q.feedback = (float) (0.2 + 0.75 * (0.5 + 0.5 * std::sin (s * 2.3)));
    q.drive = (float) (0.5 + 0.5 * std::sin (s * 0.7));
    q.eq.band[2].freq = 300.0 * std::exp2 (3.0 * std::sin (s * 5.0));
    q.eq.band[3].gainDb = 18.0 * std::sin (s * 4.0);
    q.eq.band[4].q = 0.25 * std::exp2 (5.0 * (0.5 + 0.5 * std::sin (s * 3.0)));
    q.eq.band[5].highShelf = int (s * 2) % 2 == 1;
    q.eq.band[1].on = int (s * 3.3) % 2 == 0;
    q.eq.eqOn = int (s * 0.9) % 3 != 2;
    q.lfoOn = int (s * 1.1) % 3 != 1;
    q.shape = (LfoShape) (int (s * 1.5) % 5);
    q.rateSync = int (s) % 2 == 0;
    q.rateIndex = 4 + int (s * 2) % 18;
    q.pan = (float) std::sin (s * 0.8);
    q.mix = (float) (0.3 + 0.4 * (0.5 + 0.5 * std::sin (s)));
    q.wetOnly = int (s * 0.6) % 2 == 1;
    q.bypass = int (s * 0.45) % 3 == 2;
    t.bpm = 100.0 + 40.0 * std::sin (s * 0.5);
}

// Detector de clics: mayor segunda diferencia absoluta |y[i] - 2y[i-1] + y[i-2]|.
// Un tono limpio de amplitud A da w^2·A; un salto J produce un pico de ~J.
struct ClickScore
{
    double worst = 0.0;
    size_t at = 0;
};

ClickScore clickScore (const std::vector<float>& y, size_t from)
{
    ClickScore c;
    for (size_t i = std::max<size_t> (from, 2); i < y.size(); ++i)
    {
        const double d2 = std::abs ((double) y[i] - 2.0 * y[i - 1] + y[i - 2]);
        if (d2 > c.worst)
        {
            c.worst = d2;
            c.at = i;
        }
    }
    return c;
}
} // namespace

TEST_CASE ("stability.automation_no_clicks")
{
    // Tono de 330 Hz (pico 0,3) con automatización densa de tiempo, switches, EQ, feedback,
    // LFO, MIX, WET ONLY, BYPASS y BPM, en bloques de 64.
    const double fs = 48000.0;
    const size_t n = (size_t) (fs * 12);
    const auto in = tone (n, 330.0, fs, 0.3f);
    auto p = busyParams();
    p.tape = false; // la referencia estática limpia
    const auto ref = render (p, in);
    RenderSpec spec;
    spec.block = 64;
    spec.hook = [] (DelayEngine&, EngineParams& q, TransportInfo& t, size_t pos) { automate (q, t, pos); };
    const auto out = render (p, in, spec);
    CHECK (allFinite (out));
    const auto r = clickScore (ref.L, 9600);
    const auto aL = clickScore (out.L, 9600), aR = clickScore (out.R, 9600);
    const auto& a = aL.worst >= aR.worst ? aL : aR;
    CHECK_MSG (a.worst < r.worst * 3.0, "clic %.5f en %.3f s vs referencia %.5f", a.worst, a.at / fs, r.worst);
    EVIDENCE ("2.ª diferencia máxima: automatizado %.5f (en %.3f s, canal %c) · referencia estática %.5f · x%.2f",
              a.worst, a.at / fs, aL.worst >= aR.worst ? 'L' : 'R', r.worst, a.worst / r.worst);
}

TEST_CASE ("stability.automation_peaks_with_impulses")
{
    const double fs = 48000.0;
    const size_t n = (size_t) (fs * 8);
    auto in = tone (n, 330.0, fs, 0.3f);
    for (size_t i = 24000; i < n; i += 37000)
        in.L[i] += 0.5f, in.R[i] += 0.5f;
    auto p = busyParams();
    p.feedback = 0.95f; // referencia con el feedback más alto que alcanza la automatización
    const auto ref = render (p, in);
    RenderSpec spec;
    spec.block = 64;
    spec.hook = [] (DelayEngine&, EngineParams& q, TransportInfo& t, size_t pos) { automate (q, t, pos); };
    const auto out = render (p, in, spec);
    CHECK (allFinite (out));
    const float refPeak = std::max (maxAbs (ref.L, 4800), maxAbs (ref.R, 4800));
    const float outPeak = std::max (maxAbs (out.L, 4800), maxAbs (out.R, 4800));
    CHECK_MSG (outPeak < refPeak * 2.0f, "pico %.3f vs referencia %.3f", outPeak, refPeak);
    EVIDENCE ("tono + impulsos con automatización: pico %.3f vs referencia estática (FB 95 %%) %.3f", outPeak, refPeak);
}

TEST_CASE ("stability.sample_rates_and_block_sizes")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        auto p = busyParams();
        p.lfoOn = false; // estático: la salida no depende del tamaño de bloque
        p.duck = 0.0f;
        const auto in = noise ((size_t) fs, 0.4f);
        RenderSpec spec;
        spec.fs = fs;
        spec.block = 512;
        const auto ref = render (p, in, spec);
        CHECK (allFinite (ref));
        for (int block : { 1, 16, 64, 333, 4096 })
        {
            spec.block = block;
            const auto out = render (p, in, spec);
            size_t diff = 0;
            for (size_t i = 0; i < in.size(); ++i)
                diff += (out.L[i] != ref.L[i]);
            CHECK_MSG (diff == 0, "%.0f Hz, bloque %d: %zu diferencias", fs, block, diff);
        }
        // Con LFO y DUCK: diferencia pequeña entre bloques (alineación de sub-bloques).
        auto q = busyParams();
        spec.block = 64;
        const auto a = render (q, in, spec);
        spec.block = 512;
        const auto b = render (q, in, spec);
        double err = 0.0;
        for (size_t i = 0; i < in.size(); ++i)
            err += std::pow (a.L[i] - b.L[i], 2.0);
        const double rel = db (std::sqrt (err / in.size()) / rms (b.L, 0, in.size()));
        CHECK (rel < -40.0);
        CHECK (allFinite (a));
        EVIDENCE ("%.1f kHz: estático idéntico con bloques 1/16/64/333/512/4096; con LFO+DUCK 64 vs 512 difiere %.1f dB bajo la señal",
                  fs / 1000, rel);
    }
}

TEST_CASE ("stability.offline_reproducible_and_reset")
{
    auto p = busyParams();
    p.shape = LfoShape::Random;
    const auto in = noise (96000, 0.5f);
    DelayEngine e;
    const auto a = render (p, in, {}, &e);
    const auto b = render (p, in, {}, &e); // render llama a prepare → reinicio
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (a.L[i] != b.L[i]) + (a.R[i] != b.R[i]);
    CHECK (diff == 0);
    // reset() vacía las colas.
    e.reset();
    Stereo silence (48000);
    float* ch[2] = { silence.L.data(), silence.R.data() };
    e.process (ch, 2, 48000, hostAt (120.0));
    CHECK (maxAbs (silence.L) == 0.0f);
    EVIDENCE ("dos renders offline idénticos (%zu diferencias); tras reset() la salida es silencio", diff);
}

TEST_CASE ("stability.layout_changes")
{
    // Cambia de estéreo a mono y viceversa en el mismo motor sin reservar.
    auto p = busyParams();
    DelayEngine e;
    e.prepare (48000.0, 512, 2);
    e.setParams (p);
    auto s = noise (48000, 0.5f);
    bool finite = true;
    for (size_t pos = 0; pos < s.size(); pos += 512)
    {
        float* ch[2] = { s.L.data() + pos, s.R.data() + pos };
        const int nch = (pos / 4096) % 2 ? 1 : 2;
        e.process (ch, nch, 512, hostAt (120.0));
        for (size_t i = pos; i < pos + 512; ++i)
            finite = finite && std::isfinite (s.L[i]);
    }
    CHECK (finite);
    // prepare con otra frecuencia de muestreo.
    e.prepare (96000.0, 64, 1);
    auto m = render (p, noise (9600, 0.5f), RenderSpec { 96000.0, 64, 1 }, &e);
    CHECK (allFinite (m));
}

TEST_CASE ("rt.no_allocation_in_process")
{
    auto p = busyParams();
    DelayEngine e;
    e.prepare (48000.0, 512, 2);
    e.setParams (p);
    auto s = noise (48000, 0.5f);
    const long before = allocationCount.load();
    for (size_t pos = 0; pos < s.size(); pos += 512)
    {
        float* ch[2] = { s.L.data() + pos, s.R.data() + pos };
        p.timeMsL = 100.0 + (double) pos * 0.01;
        p.eq.band[5].highShelf = (pos / 4096) % 2;
        e.setParams (p);
        e.process (ch, 2, 512, hostAt (120.0 + (double) pos * 1e-3));
    }
    const long after = allocationCount.load();
    CHECK (after == before);
    EVIDENCE ("reservas de memoria durante process(): %ld", after - before);
}

TEST_CASE ("rt.interpolator_gain_bounded")
{
    // La lectura Catmull-Rom no amplifica ninguna frecuencia (estabilidad del bucle).
    double worst = 0.0;
    for (int ti = 1; ti < 100; ++ti)
    {
        const double t = ti / 100.0;
        const double c0 = -0.5 * t * (1 - t) * (1 - t);           // xm1
        const double c1 = 1 - 2.5 * t * t + 1.5 * t * t * t;      // x0
        const double c2 = t * (0.5 + 2 * t - 1.5 * t * t);        // x1
        const double c3 = -0.5 * t * t * (1 - t);                 // x2
        for (int wi = 0; wi <= 400; ++wi)
        {
            const double w = kPi * wi / 400.0;
            std::complex<double> h = c0 * std::polar (1.0, w) + c1 + c2 * std::polar (1.0, -w) + c3 * std::polar (1.0, -2 * w);
            worst = std::max (worst, std::abs (h));
        }
    }
    CHECK (worst <= 1.0 + 1e-9);
    EVIDENCE ("ganancia máxima de la interpolación Hermite en cualquier fracción y frecuencia: %.9f", worst);
}

namespace {
// Escenario de reinicio del host: cola previa (FB 90 %) y, desde el bloque del
// reinicio, un tono nuevo con fundido de 2 ms. TIME 100 ms (4800), WET ONLY.
struct ResetRun
{
    Stereo in, out;
    size_t resetAt;
};

ResetRun runHostReset (int block, bool continuous)
{
    const double fs = 48000.0;
    auto p = cleanWetParams();
    p.feedback = 0.9f;
    p.timeMsL = p.timeMsR = 100.0;
    const size_t resetAt = (size_t) block * (size_t) (28800 / block); // ~0,6 s, alineado al bloque
    ResetRun r { Stereo ((size_t) fs * 2), Stereo(), resetAt };
    for (size_t i = 0; i < 4800; ++i) // material previo: crea la cola
        r.in.L[i] = r.in.R[i] = 0.4f * (float) std::sin (2.0 * kPi * 220.0 * (double) i / fs);
    for (size_t i = resetAt; i < r.in.size(); ++i)
    {
        const double t = (double) (i - resetAt);
        const float fade = (float) std::min (1.0, t / 96.0);
        r.in.L[i] = r.in.R[i] = fade * 0.3f * (float) std::sin (2.0 * kPi * 440.0 * t / fs);
    }
    DelayEngine e;
    e.prepare (fs, block, 2);
    e.setParams (p);
    r.out = r.in;
    for (size_t pos = 0; pos < r.out.size(); pos += (size_t) block)
    {
        if (pos == resetAt)
            e.hostReset (continuous); // el wrapper lo aplica al principio del bloque
        float* ch[2] = { r.out.L.data() + pos, r.out.R.data() + pos };
        e.process (ch, 2, (int) std::min<size_t> ((size_t) block, r.out.size() - pos), hostAt (120.0));
    }
    return r;
}
} // namespace

TEST_CASE ("stability.host_reset_keeps_new_audio")
{
    // Con el reinicio en mitad del audio, la entrada nueva se graba entera desde
    // su primera muestra y la cola anterior se funde en 10 ms sin clic.
    for (bool continuous : { true, false })
        for (int block : { 64, 512 })
        {
            const auto r = runHostReset (block, continuous);
            const size_t a = r.resetAt;
            // 1) Primer eco nuevo exacto en a + 4800 y entero (bit a bit con FB 90 % del tono nuevo).
            size_t lost = 0;
            for (size_t i = a + 4800; i < a + 9600; ++i)
                lost += r.out.L[i] != r.in.L[i - 4800];
            CHECK_MSG (lost == 0, "%s, bloque %d: %zu muestras del primer eco nuevo distintas",
                       continuous ? "continuo" : "tras pausa", block, lost);
            // 2) Sin cola anterior entre a + 10 ms y la llegada del eco nuevo.
            const float leftover = maxAbs (r.out.L, a + 480, a + 4800);
            CHECK (leftover == 0.0f);
            // 3) Tras una pausa, la cola anterior no suena en absoluto.
            const float oldTail = maxAbs (r.out.L, a, a + 480);
            if (! continuous)
                CHECK (oldTail == 0.0f);
            // 4) Con reinicio continuo, la cola se funde sin clic.
            const float before = maxStep (r.out.L, a - 2000, a);
            const float during = maxStep (r.out.L, a, a + 480);
            if (continuous)
                CHECK_MSG (during <= before * 1.1f + 1e-6f, "salto %.4f vs %.4f", during, before);
            EVIDENCE ("reinicio %s, bloque %3d: eco nuevo en +100 ms íntegro (%zu diferencias); cola previa en los primeros 10 ms: pico %.3f, salto máx. %.4f (antes %.4f); de 10 a 100 ms: %.1f",
                      continuous ? "continuo  " : "tras pausa", block, lost, oldTail, during, before, leftover);
        }
}

TEST_CASE ("stability.nonfinite_input_recovers")
{
    // Una muestra Inf y otra NaN con DUCK y AUTO activos: el bucle y los
    // detectores se recuperan; la salida vuelve a ser finita enseguida.
    auto p = cleanWetParams();
    p.wetOnly = false;
    p.mix = 0.5f;
    p.feedback = 0.6f;
    p.duck = 0.5f;
    p.lfoOn = true;
    p.lfoAuto = true;
    p.depth[kPanDepthIndex] = 1.0f;
    const double fs = 48000.0;
    auto in = tone ((size_t) fs, 330.0, fs, 0.3f);
    in.L[1000] = std::numeric_limits<float>::infinity();
    in.R[2000] = std::numeric_limits<float>::quiet_NaN();
    DelayEngine e;
    const auto out = render (p, in, {}, &e);
    bool finiteAfter = true;
    for (size_t i = 2001; i < out.size(); ++i)
        finiteAfter = finiteAfter && std::isfinite (out.L[i]) && std::isfinite (out.R[i]);
    CHECK (finiteAfter);
    CHECK (std::isfinite (e.telemetry().duckGainDb) && std::isfinite (e.telemetry().autoScale)
           && std::isfinite (e.telemetry().effectivePan));
    EVIDENCE ("Inf en la muestra 1000 y NaN en la 2000: salida finita desde la 2001; DUCK %.1f dB, escala AUTO %.2f",
              e.telemetry().duckGainDb, e.telemetry().autoScale);
}
