#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

namespace {
struct Echo
{
    char ch;
    size_t idx;
    float level;
};

std::vector<Echo> echoes (const Stereo& s, float threshold = 1.0e-3f)
{
    std::vector<Echo> out;
    for (const auto& p : findPeaks (s.L, threshold, 256))
        out.push_back ({ 'L', p.index, p.value });
    for (const auto& p : findPeaks (s.R, threshold, 256))
        out.push_back ({ 'R', p.index, p.value });
    std::sort (out.begin(), out.end(), [] (const Echo& a, const Echo& b) { return a.idx < b.idx; });
    return out;
}

void expectEchoes (const Stereo& s, const std::vector<Echo>& want, const char* label)
{
    const auto got = echoes (s);
    CHECK_MSG (got.size() >= want.size(), "%s: %zu ecos, esperados >= %zu", label, got.size(), want.size());
    for (size_t i = 0; i < std::min (got.size(), want.size()); ++i)
    {
        CHECK_MSG (got[i].ch == want[i].ch && got[i].idx == want[i].idx && std::abs (got[i].level - want[i].level) < 1e-4f,
                   "%s eco %zu: %c@%zu %.5f, esperado %c@%zu %.5f", label, i, got[i].ch, got[i].idx, got[i].level,
                   want[i].ch, want[i].idx, want[i].level);
        EVIDENCE ("%s eco %zu: canal %c, %.3f ms, nivel %.5f", label, i + 1, got[i].ch, got[i].idx / 48.0, got[i].level);
    }
}
} // namespace

TEST_CASE ("channels.stereo_independent_asymmetric")
{
    auto p = cleanWetParams();
    p.timeMsL = 100.0; // 4800
    p.timeMsR = 150.0; // 7200
    p.feedback = 0.5f;
    // Impulso solo en L, luego solo en R (asimétricos).
    expectEchoes (render (p, impulse (20000, 1.0f, 0.0f)),
                  { { 'L', 4800, 1.0f }, { 'L', 9600, 0.5f }, { 'L', 14400, 0.25f } }, "STEREO impulso L");
    expectEchoes (render (p, impulse (23000, 0.0f, 0.8f)),
                  { { 'R', 7200, 0.8f }, { 'R', 14400, 0.4f }, { 'R', 21600, 0.2f } }, "STEREO impulso R");
}

TEST_CASE ("channels.pingpong_alternation")
{
    auto p = cleanWetParams();
    p.pingPong = true;
    p.timeMsL = 100.0; // 4800
    p.timeMsR = 150.0; // 7200
    p.feedback = 0.5f;
    // Impulso en L: L tras TIME L, después R tras TIME R adicional.
    expectEchoes (render (p, impulse (40000, 1.0f, 0.0f)),
                  { { 'L', 4800, 0.5f }, { 'R', 12000, 0.25f }, { 'L', 16800, 0.125f }, { 'R', 24000, 0.0625f } },
                  "PING PONG impulso L (suma mono = 0,5)");
    // Impulso centrado (fuente mono en auxiliar estéreo): alternancia audible.
    expectEchoes (render (p, impulse (40000, 1.0f, 1.0f)),
                  { { 'L', 4800, 1.0f }, { 'R', 12000, 0.5f }, { 'L', 16800, 0.25f }, { 'R', 24000, 0.125f } },
                  "PING PONG impulso centrado");
}

TEST_CASE ("channels.pingpong_smoke_case_120bpm")
{
    // SMOKE 1: 1/8D L, 1/4 R, PING PONG, FB 35 %, impulso centrado → L 375 ms, R 875 ms.
    auto p = cleanWetParams();
    p.sync = true;
    p.pingPong = true;
    p.noteL = tempo::noteIndex ("1/8D");
    p.noteR = tempo::noteIndex ("1/4");
    p.feedback = 0.35f;
    const auto out = render (p, impulse (100000));
    const auto e = echoes (out);
    CHECK (e.size() >= 3);
    if (e.size() >= 3)
    {
        CHECK (e[0].ch == 'L' && e[0].idx == 18000 && std::abs (e[0].level - 1.0f) < 1e-5f);
        CHECK (e[1].ch == 'R' && e[1].idx == 42000 && std::abs (e[1].level - 0.35f) < 1e-5f);
        CHECK (e[2].ch == 'L' && e[2].idx == 60000 && std::abs (e[2].level - 0.1225f) < 1e-5f);
        EVIDENCE ("%c %.1f ms %.4f · %c %.1f ms %.4f · %c %.1f ms %.4f", e[0].ch, e[0].idx / 48.0, e[0].level,
                  e[1].ch, e[1].idx / 48.0, e[1].level, e[2].ch, e[2].idx / 48.0, e[2].level);
    }
}

TEST_CASE ("channels.dual_mono_input_stereo_mode")
{
    // Entrada dual mono en STEREO con tiempos distintos: cada canal a su tiempo.
    auto p = cleanWetParams();
    p.timeMsL = 100.0;
    p.timeMsR = 125.0;
    p.feedback = 0.0f;
    const auto out = render (p, impulse (10000));
    const auto pl = findPeaks (out.L, 0.5f), pr = findPeaks (out.R, 0.5f);
    CHECK (pl.size() == 1 && pl[0].index == 4800);
    CHECK (pr.size() == 1 && pr[0].index == 6000);
}

TEST_CASE ("channels.mono_engine_uses_time_l")
{
    for (bool pp : { false, true })
    {
        auto p = cleanWetParams();
        p.pingPong = pp;
        p.timeMsL = 100.0;
        p.timeMsR = 300.0;
        p.feedback = 0.5f;
        p.pan = 1.0f; // PAN neutro en mono
        RenderSpec spec;
        spec.channels = 1;
        const auto out = render (p, impulse (20000, 1.0f, 0.0f), spec);
        const auto pk = findPeaks (out.L, 1e-3f, 256);
        CHECK (pk.size() >= 3);
        if (pk.size() >= 3)
        {
            CHECK (pk[0].index == 4800 && std::abs (pk[0].value - 1.0f) < 1e-5f);
            CHECK (pk[1].index == 9600 && std::abs (pk[1].value - 0.5f) < 1e-5f);
            CHECK (pk[2].index == 14400 && std::abs (pk[2].value - 0.25f) < 1e-5f);
        }
        EVIDENCE ("mono→mono %s: ecos a 100/200/300 ms con 1, 0,5, 0,25 (TIME L, realimentación mono)", pp ? "PING PONG" : "STEREO");
    }
}

TEST_CASE ("channels.feedback_zero_and_high")
{
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 10.0; // 480 muestras
    p.feedback = 0.0f;
    auto out = render (p, impulse (4800, 1.0f, 0.0f));
    CHECK (findPeaks (out.L, 1e-6f, 64).size() == 1);

    p.feedback = 0.98f;
    out = render (p, impulse (480 * 60 + 100, 1.0f, 0.0f));
    const auto pk = findPeaks (out.L, 1e-6f, 64);
    CHECK (pk.size() >= 50);
    double worst = 0.0;
    for (size_t i = 0; i < std::min<size_t> (pk.size(), 50); ++i)
    {
        CHECK (pk[i].index == 480 * (i + 1));
        worst = std::max (worst, std::abs (pk[i].value - std::pow (0.98, (double) i)));
    }
    CHECK (worst < 1e-4);
    EVIDENCE ("FB 0 %%: un único eco. FB 98 %%: 50 ecos a k·10 ms con nivel 0,98^k (error máx. %.2e); eco 50 = %.4f",
              worst, pk.size() >= 50 ? pk[49].value : 0.0f);
}

TEST_CASE ("channels.pan_full_travel_single_channel_echo")
{
    auto p = cleanWetParams();
    p.timeMsL = p.timeMsR = 50.0; // 2400
    for (float pan : { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f })
    {
        p.pan = pan;
        const auto out = render (p, impulse (4000, 1.0f, 0.0f));
        const float l = out.L[2400], r = out.R[2400];
        const float theta = std::abs (pan) * 0.5f * 3.14159265f;
        const float expL = pan > 0 ? std::cos (theta) : 1.0f;
        const float expR = pan > 0 ? std::sin (theta) : 0.0f;
        CHECK_NEAR (l, expL, 1e-5);
        CHECK_NEAR (r, expR, 1e-5);
        CHECK_NEAR (l * l + r * r, 1.0, 1e-5); // potencia constante
        EVIDENCE ("eco solo en L, PAN %+.1f → L %.4f R %.4f", pan, l, r);
    }
    // Eco solo en R hacia la izquierda.
    p.pan = -1.0f;
    const auto out = render (p, impulse (4000, 0.0f, 1.0f));
    CHECK_NEAR (out.L[2400], 1.0, 1e-5);
    CHECK_NEAR (out.R[2400], 0.0, 1e-5);
}

TEST_CASE ("channels.clean_path_bit_exact")
{
    // EQ y LFO apagados, CLEAN, DRIVE 0, FB 0, 100 % wet: la salida es la entrada retardada exacta.
    auto p = cleanWetParams();
    p.timeMsL = 20.0;
    p.timeMsR = 30.0;
    const auto in = noise (48000, 0.7f);
    const auto out = render (p, in);
    size_t mismatches = 0;
    for (size_t i = 1440; i < in.size(); ++i)
        mismatches += (out.L[i] != in.L[i - 960]) + (out.R[i] != in.R[i - 1440]);
    CHECK (mismatches == 0);
    EVIDENCE ("ruido retardado 20/30 ms: %zu diferencias de muestra (bit a bit)", mismatches);
}
