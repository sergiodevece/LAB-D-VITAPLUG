#include "engine/EngineHarness.h"

using namespace labd;
using namespace labdtest;

TEST_CASE ("tempo.note_table_120bpm")
{
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/1"), 120.0), 2000.0, 1e-9);
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/4"), 120.0), 500.0, 1e-9);
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/8D"), 120.0), 375.0, 1e-9);
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/8T"), 120.0), 1000.0 / 6.0, 1e-9);
    // Puntillo x 3/2 y tresillo x 2/3 para las seis figuras.
    for (int den : { 1, 2, 4, 8, 16, 32 })
    {
        char n[8], d[8], t[8];
        std::snprintf (n, sizeof n, "1/%d", den);
        std::snprintf (d, sizeof d, "1/%dD", den);
        std::snprintf (t, sizeof t, "1/%dT", den);
        const double base = tempo::noteMs (tempo::noteIndex (n), 120.0);
        CHECK_NEAR (base, 2000.0 / den, 1e-9);
        CHECK_NEAR (tempo::noteMs (tempo::noteIndex (d), 120.0), base * 1.5, 1e-9);
        CHECK_NEAR (tempo::noteMs (tempo::noteIndex (t), 120.0), base * 2.0 / 3.0, 1e-9);
    }
    // Orden ascendente del mando.
    for (int i = 1; i < tempo::kNumNotes; ++i)
        CHECK (tempo::kNotes[i].quarters > tempo::kNotes[i - 1].quarters);
    EVIDENCE ("120 BPM: 1/1=%.1f ms, 1/4=%.1f ms, 1/8D=%.1f ms",
              tempo::noteMs (16, 120), tempo::noteMs (10, 120), tempo::noteMs (9, 120));
}

TEST_CASE ("tempo.range_extremes")
{
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/1D"), 20.0), 18000.0, 1e-9);
    CHECK_NEAR (tempo::kMaxDelayMs, 18000.0, 1e-9);
    CHECK_NEAR (tempo::noteMs (tempo::noteIndex ("1/32T"), 400.0), 150.0 / 12.0, 1e-9);
    CHECK (tempo::sanitiseBpm (std::nan ("")) < 0.0);
    CHECK (tempo::sanitiseBpm (0.0) < 0.0);
    CHECK (tempo::sanitiseBpm (-5.0) < 0.0);
    CHECK_NEAR (tempo::sanitiseBpm (10.0), 20.0, 0.0);
    CHECK_NEAR (tempo::sanitiseBpm (999.0), 400.0, 0.0);
    CHECK_NEAR (tempo::sanitiseBpm (133.5), 133.5, 0.0);
    EVIDENCE ("1/1D a 20 BPM = %.0f ms; 1/32T a 400 BPM = %.3f ms", tempo::noteMs (17, 20.0), tempo::noteMs (0, 400.0));
}

TEST_CASE ("tempo.rate_fast_figures")
{
    const double hz16 = tempo::rateHz (tempo::rateIndex ("1/16"), 120.0, 4, 4);
    const double hz16t = tempo::rateHz (tempo::rateIndex ("1/16T"), 120.0, 4, 4);
    const double hz32 = tempo::rateHz (tempo::rateIndex ("1/32"), 120.0, 4, 4);
    const double hz32t = tempo::rateHz (tempo::rateIndex ("1/32T"), 120.0, 4, 4);
    CHECK_NEAR (hz16, 8.0, 1e-9);
    CHECK_NEAR (hz16t, 12.0, 1e-9);
    CHECK_NEAR (hz32, 16.0, 1e-9);
    CHECK_NEAR (hz32t, 24.0, 1e-9);
    // Lento → rápido en 4/4 (1 BAR y 1/1 coinciden en 4/4).
    for (int i = 1; i < tempo::kNumRates; ++i)
        CHECK (tempo::rateCycleQuarters (i, 4, 4) <= tempo::rateCycleQuarters (i - 1, 4, 4));
    CHECK (tempo::rateIndex ("1/2") == 8);
    EVIDENCE ("120 BPM: 1/16=%.2f Hz, 1/16T=%.2f Hz, 1/32=%.2f Hz, 1/32T=%.2f Hz", hz16, hz16t, hz32, hz32t);
}

TEST_CASE ("tempo.bar_vs_whole_in_3_4")
{
    const double bar34 = tempo::rateHz (tempo::rateIndex ("1 BAR"), 120.0, 3, 4);
    const double whole = tempo::rateHz (tempo::rateIndex ("1/1"), 120.0, 3, 4);
    const double bar44 = tempo::rateHz (tempo::rateIndex ("1 BAR"), 120.0, 4, 4);
    CHECK_NEAR (bar34, 120.0 / 60.0 / 3.0, 1e-12);
    CHECK_NEAR (whole, 0.5, 1e-12);
    CHECK_NEAR (bar44, 0.5, 1e-12);
    CHECK_NEAR (tempo::rateHz (tempo::rateIndex ("8 BAR"), 120.0, 6, 8), 2.0 / (8.0 * 3.0), 1e-12);
    CHECK_NEAR (tempo::barQuarters (0, 0), 4.0, 0.0); // reserva 4/4
    EVIDENCE ("3/4 a 120 BPM: 1 BAR = %.4f Hz (%.0f ms), 1/1 = %.4f Hz (%.0f ms)", bar34, 1000.0 / bar34, whole, 1000.0 / whole);
}
