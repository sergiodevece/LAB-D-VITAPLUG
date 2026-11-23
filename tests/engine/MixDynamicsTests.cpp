#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

TEST_CASE ("mix.extremes_wet_only_output_bypass")
{
    auto p = cleanWetParams();
    p.wetOnly = false;
    p.timeMsL = p.timeMsR = 10.0;
    p.feedback = 0.3f;
    const auto in = noise (24000, 0.5f);

    // MIX 0 → seco exacto.
    p.mix = 0.0f;
    auto out = render (p, in);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (out.L[i] != in.L[i]) + (out.R[i] != in.R[i]);
    CHECK (diff == 0);

    // MIX 100 % → nada de seco (impulso en t=0 da 0) y eco a nivel 1.
    p.mix = 1.0f;
    auto imp = render (p, impulse (1000));
    CHECK (imp.L[0] == 0.0f && imp.L[480] == 1.0f);

    // MIX lineal: 25 % → 0,75 seco + 0,25 wet.
    p.mix = 0.25f;
    imp = render (p, impulse (1000));
    CHECK_NEAR (imp.L[0], 0.75, 1e-7);
    CHECK_NEAR (imp.L[480], 0.25, 1e-7);

    // WET ONLY fuerza 100 % sin tocar MIX (sigue a 0,25 en los parámetros).
    p.wetOnly = true;
    imp = render (p, impulse (1000));
    CHECK (imp.L[0] == 0.0f && imp.L[480] == 1.0f);
    CHECK_NEAR (p.mix, 0.25, 0.0);

    // OUTPUT -24 / +12 dB después de MIX.
    p.wetOnly = false;
    for (float dbv : { -24.0f, 12.0f })
    {
        p.outputDb = dbv;
        imp = render (p, impulse (1000));
        CHECK_NEAR (imp.L[0], 0.75 * std::pow (10.0, dbv / 20.0), 1e-5);
        CHECK_NEAR (imp.L[480], 0.25 * std::pow (10.0, dbv / 20.0), 1e-5);
    }

    // BYPASS: seco a ganancia unitaria, por fuera de OUTPUT.
    p.bypass = true;
    p.outputDb = 12.0f;
    out = render (p, in);
    diff = 0;
    for (size_t i = 0; i < in.size(); ++i)
        diff += (out.L[i] != in.L[i]) + (out.R[i] != in.R[i]);
    CHECK (diff == 0);
    EVIDENCE ("MIX 0: seco exacto · MIX 25 %%: 0,75/0,25 · WET ONLY: 0/1 con MIX intacto · OUTPUT -24/+12 dB tras MIX · BYPASS con OUTPUT +12: %zu diferencias", diff);
}

TEST_CASE ("mix.bypass_transition_smooth")
{
    auto p = cleanWetParams();
    p.wetOnly = false;
    p.mix = 0.6f;
    p.feedback = 0.5f;
    p.timeMsL = p.timeMsR = 7.0;
    p.outputDb = 6.0f;
    const double fs = 48000.0;
    const auto in = tone ((size_t) fs, 500.0, fs, 0.5f);
    RenderSpec spec;
    spec.hook = [] (DelayEngine&, EngineParams& q, TransportInfo&, size_t pos) {
        q.bypass = (pos / 9600) % 2 == 1;
        q.wetOnly = (pos / 14400) % 2 == 1;
    };
    const auto out = render (p, in, spec);
    const float natural = (float) (2.0 * kPi * 500.0 / fs) * maxAbs (out.L, 2000);
    const float worst = maxStep (out.L, 2000);
    CHECK_MSG (worst < natural * 1.25f, "salto %.4f vs %.4f", worst, natural);
    EVIDENCE ("BYPASS/WET ONLY cada 200/300 ms: peor salto %.4f, tono limpio del mismo pico %.4f", worst, natural);
}

TEST_CASE ("mix.drive_and_tape_character")
{
    // Entrada definida: seno 1 kHz a -6 dBFS (0,5), FEEDBACK 0, 100 % wet, 10 ms.
    const double fs = 48000.0;
    const size_t n = 48000;
    const auto in = tone (n, 1000.0, fs, 0.5f);
    struct Row { const char* name; bool tape; float drive; };
    double cleanThd = 0, cleanDrivenThd = 0, tapeThd = 0, tapeH2 = 0;
    for (const Row r : { Row { "CLEAN DRIVE 0", false, 0.0f }, Row { "CLEAN DRIVE 50", false, 0.5f },
                         Row { "CLEAN DRIVE 100", false, 1.0f }, Row { "TAPE DRIVE 0", true, 0.0f },
                         Row { "TAPE DRIVE 50", true, 0.5f }, Row { "TAPE DRIVE 100", true, 1.0f } })
    {
        auto p = cleanWetParams();
        p.timeMsL = p.timeMsR = 10.0;
        p.tape = r.tape;
        p.drive = r.drive;
        const auto out = render (p, in);
        const double h1 = toneAmplitude (out.L, n / 2, n, 1000.0, fs);
        double hs = 0.0;
        for (int h = 2; h <= 9; ++h)
        {
            const double a = toneAmplitude (out.L, n / 2, n, 1000.0 * h, fs);
            hs += a * a;
        }
        const double h2 = toneAmplitude (out.L, n / 2, n, 2000.0, fs);
        const double h3 = toneAmplitude (out.L, n / 2, n, 3000.0, fs);
        const double thd = std::sqrt (hs) / h1 * 100.0;
        EVIDENCE ("%-16s nivel fundamental %+6.2f dB · THD %6.3f %% · H2 %6.1f dB · H3 %6.1f dB", r.name, db (h1 / 0.5),
                  thd, db (h2 / h1), db (h3 / h1));
        if (! r.tape && r.drive == 0.0f) cleanThd = thd;
        if (! r.tape && r.drive == 1.0f) cleanDrivenThd = thd;
        if (r.tape && r.drive == 1.0f) { tapeThd = thd; tapeH2 = db (h2 / h1); }
    }
    CHECK (cleanThd < 0.01);
    CHECK (cleanDrivenThd > 3.0);  // DRIVE perceptible con FEEDBACK 0
    CHECK (tapeThd > 3.0);
    CHECK (tapeH2 > -40.0);        // TAPE asimétrica: armónicos pares
}

TEST_CASE ("mix.drive_keeps_echo_level_at_minus_12")
{
    // La compensación fuera del bucle mantiene el nivel del eco a -12 dBFS
    // (entrada de referencia) en todo el recorrido de DRIVE y en TAPE.
    const double fs = 48000.0;
    const size_t n = 48000;
    const auto in = tone (n, 1000.0, fs, 0.25f);
    double worst = 0.0;
    for (bool tapeMode : { false, true })
        for (float drv : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            auto p = cleanWetParams();
            p.timeMsL = p.timeMsR = 10.0;
            p.tape = tapeMode;
            p.drive = drv;
            const auto out = render (p, in);
            const double lvl = db (toneAmplitude (out.L, n / 2, n, 1000.0, fs) / 0.25);
            worst = std::max (worst, std::abs (lvl));
            EVIDENCE ("%s DRIVE %3.0f %%: eco con entrada a -12 dBFS → %+.2f dB", tapeMode ? "TAPE " : "CLEAN", drv * 100.0f, lvl);
        }
    CHECK (worst < 0.25);
    // A -30 dBFS la compensación se nota como un realce moderado (no amplifica el bucle).
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 10.0;
    p.tape = true;
    p.drive = 1.0f;
    const auto quiet = render (p, tone (n, 1000.0, fs, 0.0316f));
    const double boost = db (toneAmplitude (quiet.L, n / 2, n, 1000.0, fs) / 0.0316);
    CHECK (boost < 7.0);
    EVIDENCE ("TAPE DRIVE 100 a -30 dBFS: %+.2f dB (compensación máxima)", boost);
}

TEST_CASE ("mix.tape_evolves_on_recirculation")
{
    // Con TAPE las repeticiones se oscurecen vuelta a vuelta (filtro en el bucle).
    auto p = cleanWetParams();
    p.tape = true;
    p.feedback = 0.9f;
    p.timeMsL = p.timeMsR = 100.0;
    const auto in = noise (4800 * 6, 0.25f, 99, 2400);
    const auto out = render (p, in);
    auto brightness = [&] (int k) {
        // Relación de energía de diferencias (alta frecuencia) sobre energía total.
        double hf = 0, all = 0;
        for (size_t i = (size_t) (4800 * k) + 1; i < (size_t) (4800 * k + 2400); ++i)
        {
            hf += std::pow (out.L[i] - out.L[i - 1], 2.0);
            all += (double) out.L[i] * out.L[i];
        }
        return hf / all;
    };
    const double b1 = brightness (1), b3 = brightness (3), b5 = brightness (5);
    CHECK (b1 > b3 && b3 > b5);
    EVIDENCE ("TAPE: brillo relativo de las repeticiones 1/3/5 = %.3f / %.3f / %.3f", b1, b3, b5);
}

TEST_CASE ("dyn.duck_reduces_and_recovers")
{
    const double fs = 48000.0;
    const size_t n = (size_t) (fs * 3);
    // Fuente: tono 1 s y silencio después.
    const auto in = tone (n, 300.0, fs, 0.5f, 0, (size_t) fs);
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 250.0;
    p.feedback = 0.6f;
    p.wetOnly = true;
    auto ducked = p;
    ducked.duck = 1.0f;
    const auto a = render (p, in), b = render (ducked, in);

    // Durante la fuente (0,5-1,0 s): el wet baja.
    const double ra = rms (a.L, (size_t) (fs * 0.5), (size_t) fs);
    const double rb = rms (b.L, (size_t) (fs * 0.5), (size_t) fs);
    const double during = db (rb / ra);
    // En el hueco (1,6-2,4 s): recupera el nivel.
    const double ga = rms (a.L, (size_t) (fs * 1.6), (size_t) (fs * 2.4));
    const double gb = rms (b.L, (size_t) (fs * 1.6), (size_t) (fs * 2.4));
    const double gap = db (gb / ga);
    CHECK (during < -20.0);
    CHECK (gap > -1.0);
    // La cola del bucle es la misma: DUCK solo atenúa la salida.
    const double tail = db (rms (b.L, (size_t) (fs * 2.5), n) / rms (a.L, (size_t) (fs * 2.5), n));
    CHECK_NEAR (tail, 0.0, 0.05);
    EVIDENCE ("DUCK 100 %%: wet %.1f dB durante la fuente, %.2f dB en el hueco (+0,6-1,4 s), cola %.3f dB", during, gap, tail);

    // DUCK a cero es neutro (bit a bit).
    const auto c = render (p, in);
    size_t diff = 0;
    for (size_t i = 0; i < n; ++i)
        diff += a.L[i] != c.L[i];
    CHECK (diff == 0);
    // DUCK 50 %: atenuación intermedia.
    ducked.duck = 0.5f;
    const auto h = render (ducked, in);
    const double half = db (rms (h.L, (size_t) (fs * 0.5), (size_t) fs) / ra);
    CHECK (half < -6.0 && half > during);
    EVIDENCE ("DUCK 50 %%: %.1f dB durante la fuente", half);
}
