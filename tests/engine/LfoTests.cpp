#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

namespace {
EngineParams lfoParams()
{
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 10.0;
    p.eq.eqOn = true;
    const double f[6] = { 60, 150, 500, 1500, 4000, 9000 };
    for (int b = 0; b < 6; ++b)
    {
        p.eq.band[(size_t) b].on = true;
        p.eq.band[(size_t) b].freq = f[b];
        p.eq.band[(size_t) b].gainDb = 6.0;
        p.eq.band[(size_t) b].q = 1.0;
    }
    p.lfoOn = true;
    p.shape = LfoShape::Sine;
    return p;
}

struct Trace
{
    std::vector<double> value, pan;
    std::vector<std::array<double, 6>> freq;
    std::vector<float> gate;
};

// Registra la telemetría cada 16 muestras (bloques de 16).
Trace trace (const EngineParams& p, double seconds, TransportInfo t = hostAt (120.0), int channels = 2,
             const std::function<void (EngineParams&, size_t)>& hook = {}, float inputLevel = 0.0f)
{
    DelayEngine e;
    const double fs = 48000.0;
    e.prepare (fs, 16, channels);
    EngineParams q = p;
    Trace tr;
    float l[16], r[16];
    const size_t blocks = (size_t) (seconds * fs / 16);
    for (size_t k = 0; k < blocks; ++k)
    {
        if (hook)
            hook (q, k * 16);
        for (int i = 0; i < 16; ++i)
            l[i] = r[i] = inputLevel * (float) std::sin (2.0 * kPi * 330.0 * (double) (k * 16 + (size_t) i) / fs);
        float* ch[2] = { l, r };
        e.setParams (q);
        e.process (ch, channels, 16, t);
        t.ppq += 16.0 / fs * t.bpm / 60.0;
        const auto& tl = e.telemetry();
        tr.value.push_back (tl.lfoValue);
        tr.pan.push_back (tl.effectivePan);
        std::array<double, 6> f {};
        for (int b = 0; b < 6; ++b)
            f[(size_t) b] = tl.eqEffectiveFreq[(size_t) b];
        tr.freq.push_back (f);
        tr.gate.push_back (tl.lfoGate);
    }
    return tr;
}

double measuredHz (const std::vector<double>& v, double seconds, size_t skip = 0)
{
    int crossings = 0;
    size_t first = 0, last = 0;
    for (size_t i = skip + 1; i < v.size(); ++i)
        if (v[i - 1] < 0.0 && v[i] >= 0.0)
        {
            if (crossings == 0)
                first = i;
            last = i;
            ++crossings;
        }
    if (crossings < 2)
        return 0.0;
    const double dt = (double) (last - first) * seconds / (double) v.size();
    return (crossings - 1) / dt;
}
} // namespace

TEST_CASE ("lfo.rate_free_and_sync_measured")
{
    auto p = lfoParams();
    p.depth[kPanDepthIndex] = 1.0f;
    struct Row { const char* label; bool sync; double hz; int idx; double expect; };
    const Row rows[] = {
        { "libre 0,50 Hz", false, 0.5, 0, 0.5 },     { "libre 20 Hz", false, 20.0, 0, 20.0 },
        { "libre 35 Hz → 20", false, 35.0, 0, 20.0 }, { "1/16", true, 0, tempo::rateIndex ("1/16"), 8.0 },
        { "1/16T", true, 0, tempo::rateIndex ("1/16T"), 12.0 }, { "1/32", true, 0, tempo::rateIndex ("1/32"), 16.0 },
        { "1/32T", true, 0, tempo::rateIndex ("1/32T"), 24.0 }, { "1/2", true, 0, tempo::rateIndex ("1/2"), 1.0 },
    };
    for (const auto& r : rows)
    {
        p.rateSync = r.sync;
        p.rateHz = r.hz;
        p.rateIndex = r.idx;
        const double secs = r.expect < 2.0 ? 8.0 : 2.0;
        const auto tr = trace (p, secs);
        const double hz = measuredHz (tr.value, secs);
        CHECK_MSG (std::abs (hz - r.expect) < r.expect * 0.01, "%s: %.4f Hz", r.label, hz);
        EVIDENCE ("RATE %-16s a 120 BPM: medido %.4f Hz (esperado %.2f)", r.label, hz, r.expect);
    }
}

TEST_CASE ("lfo.time_ms_with_rate_1_32T")
{
    // TIME L = 350 ms libre junto con RATE = 1/32T sincronizado.
    auto p = lfoParams();
    p.sync = false;
    p.timeMsL = 350.0;
    p.rateSync = true;
    p.rateIndex = tempo::rateIndex ("1/32T");
    p.depth[kPanDepthIndex] = 0.2f;
    const auto tr = trace (p, 2.0);
    const double hz = measuredHz (tr.value, 2.0);
    CHECK_NEAR (hz, 24.0, 0.24);
    DelayEngine e;
    e.prepare (48000.0, 16, 2);
    e.setParams (p);
    float l[16] {}, r[16] {};
    float* ch[2] = { l, r };
    e.process (ch, 2, 16, hostAt (120.0));
    CHECK_NEAR (e.telemetry().effectiveTimeMs[0], 350.0, 0.0);
    EVIDENCE ("TIME L 350 ms + RATE 1/32T: LFO %.3f Hz, tiempo efectivo %.1f ms", hz, e.telemetry().effectiveTimeMs[0]);
}

TEST_CASE ("lfo.bar_vs_whole_3_4_measured")
{
    auto p = lfoParams();
    p.rateSync = true;
    p.depth[0] = 1.0f;
    auto t = hostAt (120.0);
    t.sigNumerator = 3;
    t.sigDenominator = 4;
    p.rateIndex = tempo::rateIndex ("1 BAR");
    const double bar = measuredHz (trace (p, 12.0, t).value, 12.0);
    p.rateIndex = tempo::rateIndex ("1/1");
    const double whole = measuredHz (trace (p, 12.0, t).value, 12.0);
    CHECK_NEAR (bar, 2.0 / 3.0, 0.01);
    CHECK_NEAR (whole, 0.5, 0.01);
    // Sin compás del host: reserva 4/4.
    t.hasTimeSig = false;
    p.rateIndex = tempo::rateIndex ("1 BAR");
    const double fallback = measuredHz (trace (p, 12.0, t).value, 12.0);
    CHECK_NEAR (fallback, 0.5, 0.01);
    EVIDENCE ("3/4 a 120 BPM: 1 BAR %.4f Hz, 1/1 %.4f Hz; sin compás (4/4) 1 BAR %.4f Hz", bar, whole, fallback);
}

TEST_CASE ("lfo.each_destination_and_all_together")
{
    const double base[6] = { 60, 150, 500, 1500, 4000, 9000 };
    for (int dest = 0; dest <= 7; ++dest) // 0-5 bandas, 6 PAN, 7 todos
    {
        auto p = lfoParams();
        p.rateHz = 2.0;
        for (int i = 0; i < kNumLfoDepths; ++i)
            p.depth[(size_t) i] = (dest == 7 || dest == i) ? 1.0f : 0.0f;
        const auto tr = trace (p, 2.0);
        for (int b = 0; b < 6; ++b)
        {
            double lo = 1e9, hi = 0;
            for (size_t k = 9000 / 16 * 0 + 3000; k < tr.freq.size(); ++k)
            {
                lo = std::min (lo, tr.freq[k][(size_t) b]);
                hi = std::max (hi, tr.freq[k][(size_t) b]);
            }
            const bool active = dest == 7 || dest == b;
            if (active)
            {
                const double expLo = std::max (20.0, base[b] * std::exp2 (-1.5));
                const double expHi = std::min (20000.0, base[b] * std::exp2 (1.5));
                CHECK_MSG (std::abs (std::log2 (lo / expLo)) < 0.03 && std::abs (std::log2 (hi / expHi)) < 0.03,
                           "banda %d: %.1f..%.1f, esperado %.1f..%.1f", b + 1, lo, hi, expLo, expHi);
                if (dest != 7)
                    EVIDENCE ("destino %d: banda %d recorre %.1f-%.1f Hz (base %.0f, ±1,5 oct)", dest + 1, b + 1, lo, hi, base[b]);
            }
            else
            {
                CHECK_MSG (lo == hi && std::abs (lo - base[b]) < 1e-6, "banda %d sin profundidad se movió: %.4f..%.4f", b + 1, lo, hi);
            }
        }
        double plo = 1, phi = -1;
        for (size_t k = 3000; k < tr.pan.size(); ++k)
        {
            plo = std::min (plo, tr.pan[k]);
            phi = std::max (phi, tr.pan[k]);
        }
        if (dest == 6 || dest == 7)
        {
            CHECK (plo < -0.98 && phi > 0.98);
            EVIDENCE ("%s: PAN recorre %.3f..%.3f", dest == 6 ? "destino PAN" : "todos", plo, phi);
        }
        else
            CHECK (plo == 0.0 && phi == 0.0);
    }
}

TEST_CASE ("lfo.zero_depth_neutral")
{
    auto off = lfoParams();
    off.lfoOn = false;
    off.feedback = 0.4f;
    off.pan = 0.3f;
    auto on = off;
    on.lfoOn = true;
    on.shape = LfoShape::Square;
    on.rateHz = 7.0; // profundidades a 0
    const auto in = noise (48000, 0.5f);
    const auto a = render (off, in), b = render (on, in);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (a.L[i] != b.L[i]) + (a.R[i] != b.R[i]);
    CHECK (diff == 0);
    EVIDENCE ("LFO encendido con profundidades 0 %% frente a apagado: %zu muestras distintas", diff);
}

TEST_CASE ("lfo.on_off_ramps")
{
    auto p = lfoParams();
    p.rateHz = 0.25;
    p.phaseDeg = 90.0; // seno en +1
    p.depth[kPanDepthIndex] = 1.0f;
    const auto tr = trace (p, 0.6, hostAt (120.0), 2, [] (EngineParams& q, size_t pos) { q.lfoOn = pos < 14400; });
    // Apagado en 0,3 s: la profundidad efectiva baja en ~80 ms, no de golpe.
    const size_t k0 = 14400 / 16;
    CHECK (tr.gate[k0 + 1] > 0.9f);
    CHECK (tr.gate[k0 + 120] < 0.5f && tr.gate[k0 + 120] > 0.0f);
    CHECK (tr.gate.back() == 0.0f && tr.pan.back() == 0.0);
    EVIDENCE ("OFF: compuerta %.3f a +0,3 ms, %.3f a +40 ms, %.3f al final; PAN final %.3f",
              tr.gate[k0 + 1], tr.gate[k0 + 120], tr.gate.back(), tr.pan.back());
}

TEST_CASE ("lfo.square_saw_edges_smoothed")
{
    auto p = lfoParams();
    p.rateHz = 2.0;
    p.depth[kPanDepthIndex] = 1.0f;
    for (auto shape : { LfoShape::Square, LfoShape::Saw })
    {
        p.shape = shape;
        const auto tr = trace (p, 2.0);
        double worst = 0.0;
        for (size_t k = 1; k < tr.pan.size(); ++k)
            worst = std::max (worst, std::abs (tr.pan[k] - tr.pan[k - 1]));
        // Arista de 2 unidades suavizada: < 0,25 por cada 16 muestras.
        CHECK (worst < 0.25);
        double lo = 1, hi = -1;
        for (double v : tr.value)
        {
            lo = std::min (lo, v);
            hi = std::max (hi, v);
        }
        // Conserva el carácter: la cuadrada llega a ±1; la sierra pierde solo el
        // retardo del suavizado en su pico (pendiente · tau).
        const double reach = shape == LfoShape::Square ? 0.99 : 0.90;
        CHECK (lo < -reach && hi > reach);
        EVIDENCE ("%s 2 Hz: salto máx. de PAN por 16 muestras %.3f; extremos %.3f..%.3f",
                  shape == LfoShape::Square ? "SQR" : "SAW", worst, lo, hi);
    }
}

TEST_CASE ("lfo.random_reproducible_and_phase")
{
    auto p = lfoParams();
    p.shape = LfoShape::Random;
    p.rateHz = 5.0;
    p.depth[2] = 1.0f;
    p.depth[kPanDepthIndex] = 0.7f;
    p.feedback = 0.5f;
    const auto in = noise (96000, 0.5f);
    const auto a = render (p, in), b = render (p, in);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (a.L[i] != b.L[i]) + (a.R[i] != b.R[i]);
    CHECK (diff == 0);
    for (int i = 0; i < 1000; ++i)
    {
        const float v = lfoRandomLevel (i, kLfoDefaultSeed);
        CHECK (v >= -1.0f && v <= 1.0f);
    }
    CHECK (lfoRandomLevel (3, kLfoDefaultSeed) != lfoRandomLevel (4, kLfoDefaultSeed));
    // PHASE: seno con 90° empieza en +1.
    CHECK_NEAR (lfoWave (LfoShape::Sine, 0.25, 0), 1.0, 1e-6);
    CHECK_NEAR (lfoWave (LfoShape::Triangle, 0.25, 0), 1.0, 1e-6);
    CHECK_NEAR (lfoWave (LfoShape::Triangle, 0.0, 0), 0.0, 1e-6);
    EVIDENCE ("RND con semilla fija: dos renders offline con %zu diferencias", diff);
}

TEST_CASE ("lfo.auto_mode_follower")
{
    auto p = lfoParams();
    p.lfoAuto = true;
    p.shape = LfoShape::Saw; // AUTO usa seno de un compás e ignora la forma MANUAL
    p.rateHz = 9.0;
    p.depth[kPanDepthIndex] = 1.0f;
    const auto quiet = trace (p, 6.0, hostAt (120.0), 2, {}, 0.0f);
    const auto loud = trace (p, 6.0, hostAt (120.0), 2, {}, 0.5f);
    const double hz = measuredHz (quiet.value, 6.0);
    CHECK_NEAR (hz, 0.5, 0.01); // 1 compás a 120 BPM (4/4)
    double qmax = 0, lmax = 0;
    for (size_t k = 3000; k < quiet.pan.size(); ++k)
    {
        qmax = std::max (qmax, std::abs (quiet.pan[k]));
        lmax = std::max (lmax, std::abs (loud.pan[k]));
    }
    CHECK (qmax > 0.98);
    CHECK (lmax < 0.25);
    EVIDENCE ("AUTO: %.3f Hz (1 compás); PAN máx. sin señal %.3f, con señal a -6 dBFS %.3f", hz, qmax, lmax);

    // Recupera la profundidad en la cola.
    const auto tail = trace (p, 4.0, hostAt (120.0), 2, {}, 0.0f);
    (void) tail;
    DelayEngine e;
    e.prepare (48000.0, 16, 2);
    e.setParams (p);
    float l[16], r[16];
    float* ch[2] = { l, r };
    float scaleLoud = 1.0f, scaleTail = 0.0f;
    for (int k = 0; k < 9000; ++k)
    {
        const float lvl = k < 6000 ? 0.5f : 0.0f;
        for (int i = 0; i < 16; ++i)
            l[i] = r[i] = lvl * (float) std::sin (0.04 * (k * 16 + i));
        e.process (ch, 2, 16, hostAt (120.0));
        if (k == 5999)
            scaleLoud = e.telemetry().autoScale;
    }
    scaleTail = e.telemetry().autoScale;
    CHECK (scaleLoud < 0.2f && scaleTail > 0.9f);
    EVIDENCE ("AUTO: escala de profundidad %.3f con señal, %.3f 1 s después de callar", scaleLoud, scaleTail);
}

TEST_CASE ("lfo.bands_off_no_modulation_pan_still_works")
{
    auto p = lfoParams();
    p.feedback = 0.3f;
    p.rateHz = 3.0;
    p.eq.eqOn = false;
    auto withDepth = p;
    withDepth.depth[2] = 1.0f;
    withDepth.depth[4] = 1.0f;
    const auto in = noise (48000, 0.5f);
    const auto a = render (p, in), b = render (withDepth, in);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (a.L[i] != b.L[i]);
    CHECK (diff == 0);
    // Banda apagada individualmente: tampoco modula.
    auto bandOff = lfoParams();
    bandOff.eq.band[2].on = false;
    auto bandOffMod = bandOff;
    bandOffMod.depth[2] = 1.0f;
    const auto c = render (bandOff, in), d = render (bandOffMod, in);
    size_t diff2 = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff2 += (c.L[i] != d.L[i]);
    CHECK (diff2 == 0);
    // PAN sigue modulando con el EQ en bypass.
    withDepth.depth[kPanDepthIndex] = 1.0f;
    const auto tr = trace (withDepth, 2.0);
    double phi = 0;
    for (double v : tr.pan)
        phi = std::max (phi, std::abs (v));
    CHECK (phi > 0.98);
    EVIDENCE ("EQ IN apagado: modular bandas cambia %zu muestras; banda 3 apagada: %zu; PAN sigue llegando a %.3f", diff, diff2, phi);
}

TEST_CASE ("lfo.host_resync_and_bpm_continuity")
{
    auto p = lfoParams();
    p.rateSync = true;
    p.rateIndex = tempo::rateIndex ("1/4");
    p.depth[kPanDepthIndex] = 1.0f;
    DelayEngine e;
    e.prepare (48000.0, 16, 2);
    e.setParams (p);
    float l[16] {}, r[16] {};
    float* ch[2] = { l, r };
    // Arranque en ppq 10,25: la fase se alinea (10,25 negras / 1 negra → 0,25).
    auto t = hostAt (120.0, true, 10.25);
    e.process (ch, 2, 16, t);
    const double ph0 = e.telemetry().lfoPhase;
    CHECK_NEAR (ph0, 0.25 + 16.0 / 24000.0, 1e-6);
    // Cambio de BPM normal: la fase sigue continua (sin realinear).
    double last = e.telemetry().lfoPhase;
    double worstJump = 0.0;
    for (int k = 0; k < 3000; ++k)
    {
        t.ppq += 16.0 / 48000.0 * t.bpm / 60.0;
        t.bpm = 120.0 + 30.0 * std::sin (k * 0.002);
        e.process (ch, 2, 16, t);
        double d = e.telemetry().lfoPhase - last;
        d -= std::floor (d);
        worstJump = std::max (worstJump, d);
        last = e.telemetry().lfoPhase;
    }
    CHECK (worstJump < 0.003);
    // Salto de posición (ciclo): se realinea con la posición musical.
    t.ppq = 33.5;
    e.process (ch, 2, 16, t);
    CHECK_NEAR (e.telemetry().lfoPhase, 0.5 + 16.0 / 48000.0 * t.bpm / 60.0, 1e-3);
    EVIDENCE ("arranque ppq 10,25 → fase %.4f; BPM variable: avance máx. %.5f ciclos/16 muestras; locate a 33,5 → fase %.4f",
              ph0, worstJump, e.telemetry().lfoPhase);
}
