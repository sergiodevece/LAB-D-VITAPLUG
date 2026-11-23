#include "engine/TempoMath.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace labd::tempo {

namespace {
constexpr double n (int den) { return 4.0 / den; }
constexpr double d (int den) { return 4.0 / den * 1.5; }
constexpr double t (int den) { return 4.0 / den * 2.0 / 3.0; }
} // namespace

const Note kNotes[kNumNotes] = {
    { "1/32T", t (32) }, { "1/32", n (32) },  { "1/16T", t (16) }, { "1/32D", d (32) },
    { "1/16", n (16) },  { "1/8T", t (8) },   { "1/16D", d (16) }, { "1/8", n (8) },
    { "1/4T", t (4) },   { "1/8D", d (8) },   { "1/4", n (4) },    { "1/2T", t (2) },
    { "1/4D", d (4) },   { "1/2", n (2) },    { "1/1T", t (1) },   { "1/2D", d (2) },
    { "1/1", n (1) },    { "1/1D", d (1) },
};

const RateChoice kRates[kNumRates] = {
    { "8 BAR", 0.0, 8 },   { "4 BAR", 0.0, 4 },   { "2 BAR", 0.0, 2 },   { "1/1D", d (1), 0 },
    { "1 BAR", 0.0, 1 },   { "1/1", n (1), 0 },   { "1/2D", d (2), 0 },  { "1/1T", t (1), 0 },
    { "1/2", n (2), 0 },   { "1/4D", d (4), 0 },  { "1/2T", t (2), 0 },  { "1/4", n (4), 0 },
    { "1/8D", d (8), 0 },  { "1/4T", t (4), 0 },  { "1/8", n (8), 0 },   { "1/16D", d (16), 0 },
    { "1/8T", t (8), 0 },  { "1/16", n (16), 0 }, { "1/32D", d (32), 0 }, { "1/16T", t (16), 0 },
    { "1/32", n (32), 0 }, { "1/32T", t (32), 0 },
};

int noteIndex (const char* label) noexcept
{
    for (int i = 0; i < kNumNotes; ++i)
        if (std::strcmp (kNotes[i].label, label) == 0)
            return i;
    return -1;
}

int rateIndex (const char* label) noexcept
{
    for (int i = 0; i < kNumRates; ++i)
        if (std::strcmp (kRates[i].label, label) == 0)
            return i;
    return -1;
}

double noteMs (int noteIdx, double bpm) noexcept
{
    noteIdx = std::clamp (noteIdx, 0, kNumNotes - 1);
    return kNotes[noteIdx].quarters * quarterMs (bpm);
}

double barQuarters (int num, int den) noexcept
{
    if (num <= 0 || den <= 0)
        return 4.0; // reserva 4/4
    return (double) num * 4.0 / (double) den;
}

double rateCycleQuarters (int rateIdx, int num, int den) noexcept
{
    rateIdx = std::clamp (rateIdx, 0, kNumRates - 1);
    const auto& r = kRates[rateIdx];
    return r.bars > 0 ? (double) r.bars * barQuarters (num, den) : r.quarters;
}

double rateHz (int rateIdx, double bpm, int num, int den) noexcept
{
    return (bpm / 60.0) / rateCycleQuarters (rateIdx, num, den);
}

double sanitiseBpm (double bpm) noexcept
{
    if (! std::isfinite (bpm) || bpm <= 0.0)
        return -1.0;
    return std::clamp (bpm, kMinBpm, kMaxBpm);
}

} // namespace labd::tempo
