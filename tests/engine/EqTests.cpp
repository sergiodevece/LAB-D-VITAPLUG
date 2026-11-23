#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

namespace {
EqParams flatEq()
{
    EqParams eq;
    eq.eqOn = true;
    const double f[6] = { 30, 120, 400, 1500, 5000, 12000 };
    for (int b = 0; b < 6; ++b)
    {
        eq.band[(size_t) b].freq = f[b];
        eq.band[(size_t) b].gainDb = 0.0;
        eq.band[(size_t) b].q = 0.7071;
        eq.band[(size_t) b].on = (b != 0 && b != 5);
    }
    return eq;
}

// Ganancia medida (dB) de un tono a través del EchoEq.
double measureEq (const EqParams& eq, double freq, double fs, double* predictedDb = nullptr)
{
    EchoEq e;
    e.prepare (fs, 2);
    e.setParams (eq);
    const size_t n = (size_t) (fs * 1.0);
    auto s = tone (n, freq, fs, 0.25f);
    double ones[6] = { 1, 1, 1, 1, 1, 1 };
    for (size_t pos = 0; pos < n; pos += 16)
    {
        const int len = (int) std::min<size_t> (16, n - pos);
        e.updateBlock (ones, len);
        e.process (s.L.data() + pos, s.R.data() + pos, len);
    }
    if (predictedDb != nullptr)
    {
        std::complex<double> h = 1.0;
        for (int b = 0; b < 6; ++b)
            if (e.bandAmount (b) > 0.0f)
                h *= svfResponse (e.coefficients (b), freq, fs);
        *predictedDb = db (std::abs (h));
    }
    return db (toneAmplitude (s.L, n / 2, n, freq, fs) / 0.25);
}
} // namespace

TEST_CASE ("eq.band_types_tone_response")
{
    const double fs = 48000.0;
    struct Row { const char* name; int band; double freq, gain, q; bool shelf; double probe, expectDb, tol; };
    const Row rows[] = {
        { "LOW CUT 100 Hz Q0,707", 0, 100, 0, 0.7071, false, 50, -12.3, 0.6 },
        { "LOW CUT 100 Hz en corte", 0, 100, 0, 0.7071, false, 100, -3.01, 0.05 },
        { "LOW SHELF +6 dB 200 Hz", 1, 200, 6, 0.7071, false, 25, 6.0, 0.15 },
        { "BELL +12 dB 1 kHz Q2", 2, 1000, 12, 2.0, false, 1000, 12.0, 0.02 },
        { "BELL -9 dB 3 kHz Q1", 3, 3000, -9, 1.0, false, 3000, -9.0, 0.02 },
        { "BELL +18 dB 6 kHz Q8", 4, 6000, 18, 8.0, false, 6000, 18.0, 0.05 },
        { "HIGH CUT 2 kHz", 5, 2000, 0, 0.7071, false, 2000, -3.01, 0.05 },
        { "HIGH CUT 2 kHz a 8 kHz", 5, 2000, 0, 0.7071, false, 8000, -25.7, 0.3 }, // bilineal: más pendiente que el prototipo analógico (-24,1)
        { "HIGH SHELF -6 dB 4 kHz", 5, 4000, -6, 0.7071, true, 18000, -6.0, 0.3 },
    };
    for (const auto& r : rows)
    {
        EqParams eq = flatEq();
        for (auto& b : eq.band)
            b.on = false;
        auto& b = eq.band[(size_t) r.band];
        b.on = true;
        b.freq = r.freq;
        b.gainDb = r.gain;
        b.q = r.q;
        b.highShelf = r.shelf;
        double predicted = 0.0;
        const double measured = measureEq (eq, r.probe, fs, &predicted);
        CHECK_MSG (std::abs (measured - predicted) < 0.05, "%s: medido %.3f dB, curva %.3f dB", r.name, measured, predicted);
        CHECK_MSG (std::abs (measured - r.expectDb) < r.tol, "%s: medido %.3f dB, esperado %.2f", r.name, measured, r.expectDb);
        EVIDENCE ("%-26s tono %6.0f Hz: medido %+7.3f dB · curva del editor %+7.3f dB", r.name, r.probe, measured, predicted);
    }
}

TEST_CASE ("eq.flat_bands_are_identity")
{
    EqParams eq = flatEq();
    EchoEq e;
    e.prepare (48000.0, 2);
    e.setParams (eq);
    auto s = noise (4096, 0.5f);
    const auto ref = s;
    double ones[6] = { 1, 1, 1, 1, 1, 1 };
    for (size_t pos = 0; pos < s.size(); pos += 16)
    {
        e.updateBlock (ones, 16);
        e.process (s.L.data() + pos, s.R.data() + pos, 16);
    }
    double worst = 0.0;
    for (size_t i = 0; i < s.size(); ++i)
        worst = std::max (worst, (double) std::abs (s.L[i] - ref.L[i]));
    CHECK (worst < 1e-6);
    EVIDENCE ("INIT del EQ (bandas 2-5 a 0 dB, 1 y 6 fuera): diferencia máxima %.2e", worst);
}

TEST_CASE ("eq.only_wet_and_loop_gain_unchanged")
{
    // MIX 50 %: la parte seca no cambia con un realce fuerte; la ganancia por
    // vuelta del bucle es la misma con EQ plano y con realce de salida.
    auto base = cleanWetParams();
    base.wetOnly = false;
    base.mix = 0.5f;
    base.timeMsL = base.timeMsR = 100.0; // 4800
    base.feedback = 0.6f;
    base.eq = flatEq();

    auto boosted = base;
    boosted.eq.band[2].gainDb = 12.0;
    boosted.eq.band[2].freq = 1000.0;
    boosted.eq.band[2].q = 1.0;
    boosted.eq.band[5].on = true;
    boosted.eq.band[5].freq = 3000.0;

    const auto a = render (base, impulse (4800 * 4));
    const auto b = render (boosted, impulse (4800 * 4));
    CHECK (a.L[0] == 0.5f && b.L[0] == 0.5f); // seco idéntico

    auto ratio = [] (const Stereo& s) {
        const double e1 = energy (s.L, 4800, 9600), e2 = energy (s.L, 9600, 14400);
        return std::sqrt (e2 / e1);
    };
    const double ra = ratio (a), rb = ratio (b);
    CHECK_NEAR (ra, 0.6, 2e-3);
    CHECK_NEAR (rb, 0.6, 2e-3);
    EVIDENCE ("seco en t=0: plano %.3f, realce %.3f · ganancia por vuelta: plano %.5f, realce +12 dB %.5f (FB 0,6)",
              a.L[0], b.L[0], ra, rb);

    // MIX 0: la salida es exactamente la entrada aunque el EQ tenga realces.
    boosted.mix = 0.0f;
    const auto in = noise (24000, 0.5f);
    const auto out = render (boosted, in);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += out.L[i] != in.L[i];
    CHECK (diff == 0);
}

TEST_CASE ("eq.frequency_clamped_below_nyquist")
{
    EchoEq e;
    e.prepare (44100.0, 2);
    EqParams eq = flatEq();
    eq.band[4].freq = 20000.0;
    eq.band[0].freq = 5.0;
    eq.band[0].on = true;
    e.setParams (eq);
    double mult[6] = { 1, 1, 1, 1, 4.0, 1 };
    e.updateBlock (mult, 16);
    CHECK_NEAR (e.effectiveFrequency (4), 0.45 * 44100.0, 1e-6);
    CHECK_NEAR (e.effectiveFrequency (0), 20.0, 1e-9);
    EVIDENCE ("44,1 kHz: banda 5 a 20 kHz x4 → %.1f Hz (0,45·fs); banda 1 a 5 Hz → %.1f Hz", e.effectiveFrequency (4), e.effectiveFrequency (0));
}

TEST_CASE ("eq.extreme_modulation_stable")
{
    // Barrido rápido 20 Hz ↔ 20 kHz con Q 8 y +18 dB sobre ruido: finito y acotado.
    const double fs = 48000.0;
    EchoEq e;
    e.prepare (fs, 2);
    EqParams eq = flatEq();
    for (auto& b : eq.band)
    {
        b.on = true;
        b.q = 8.0;
        b.gainDb = 18.0;
        b.freq = 632.0;
    }
    e.setParams (eq);
    auto s = noise ((size_t) fs * 2, 0.25f);
    float peak = 0.0f;
    for (size_t pos = 0; pos < s.size(); pos += 16)
    {
        const double ph = 2.0 * kPi * 30.0 * (double) pos / fs; // 30 Hz, ±5 octavas
        double m[6];
        for (int b = 0; b < 6; ++b)
            m[b] = std::exp2 (5.0 * std::sin (ph + b));
        e.updateBlock (m, 16);
        e.process (s.L.data() + pos, s.R.data() + pos, 16);
        peak = std::max (peak, maxAbs (s.L, pos, pos + 16));
    }
    CHECK (allFinite (s));
    CHECK (peak < 2000.0f);
    EVIDENCE ("6 bandas Q8 +18 dB moduladas ±5 oct a 30 Hz: pico %.1f (cascada de 6 x +18 dB = x%.0f), finito", peak, std::pow (10.0, 108.0 / 20.0));
}

TEST_CASE ("eq.switches_smooth")
{
    // IN de banda, EQ IN y HIGH CUT ↔ HIGH SHELF sobre un tono: sin saltos.
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 5.0;
    p.eq = flatEq();
    p.eq.band[5].on = true;
    p.eq.band[5].freq = 3000.0;
    p.eq.band[5].gainDb = -12.0;
    p.eq.band[2].gainDb = 12.0;
    const double fs = 48000.0;
    const auto in = tone ((size_t) fs * 2, 2500.0, fs, 0.4f);
    const auto ref = render (p, in);
    RenderSpec spec;
    spec.hook = [] (DelayEngine&, EngineParams& q, TransportInfo&, size_t pos) {
        const int k = (int) (pos / 4800);
        q.eq.band[5].highShelf = (k % 2) == 1;
        q.eq.band[2].on = (k % 3) != 2;
        q.eq.eqOn = (k % 7) != 6;
    };
    const auto out = render (p, in, spec);
    // Referencia: el mayor salto posible de un tono de 2,5 kHz con la amplitud
    // máxima observada (una sinusoide limpia de esa amplitud).
    const float natural = (float) (2.0 * kPi * 2500.0 / fs) * maxAbs (out.L, 4800);
    const float worst = maxStep (out.L, 4800);
    CHECK_MSG (worst < natural * 1.25f, "salto %.4f vs %.4f", worst, natural);
    EVIDENCE ("conmutaciones IN/EQ IN/tipo cada 100 ms: peor salto %.4f, tono limpio de igual pico %.4f (estático %.4f)",
              worst, natural, maxStep (ref.L, 4800));
}
