#include "engine/EchoEq.h"

#include "engine/Smoothers.h"

#include <algorithm>
#include <cmath>

namespace labd {

namespace {
constexpr double kParamTau = 0.020;  // suavizado de FREQ/GAIN/Q
constexpr double kSwitchTime = 0.020; // fundido de IN, EQ IN y cambio de tipo
} // namespace

FilterType bandFilterType (int bandIndex, bool highShelf) noexcept
{
    switch (bandIndex)
    {
        case 0: return FilterType::LowCut;
        case 1: return FilterType::LowShelf;
        case 5: return highShelf ? FilterType::HighShelf : FilterType::HighCut;
        default: return FilterType::Bell;
    }
}

void EchoEq::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = std::clamp (numChannels, 1, 2);
    reset();
}

void EchoEq::reset() noexcept
{
    for (auto& ch : state)
        for (auto& s : ch)
            s.reset();
    first = true;
}

void EchoEq::updateBlock (const double* mult, int numSamples) noexcept
{
    const double a = first ? 1.0 : onePoleCoefficient (kParamTau, fs / std::max (1, numSamples));
    const float step = (float) (numSamples / (kSwitchTime * fs));

    globalPrev = globalNow;
    const float globalTarget = params.eqOn ? 1.0f : 0.0f;
    if (first)
        globalNow = globalTarget;
    else
        globalNow = globalNow < globalTarget ? std::min (globalTarget, globalNow + step)
                                             : std::max (globalTarget, globalNow - step);
    if (first)
        globalPrev = globalNow;

    for (int b = 0; b < kNumEqBands; ++b)
    {
        const auto& p = params.band[(size_t) b];
        const auto wantedType = bandFilterType (b, p.highShelf);
        const auto bi = (size_t) b;

        // Rango de FREQ sujeto al margen bajo Nyquist; Q 0,25-8; GAIN ±18 dB.
        const double fT = std::log (clampEqFreq (p.freq, fs));
        const double gT = std::clamp (p.gainDb, -18.0, 18.0);
        const double qT = std::log (std::clamp (p.q, 0.25, 8.0));

        if (first)
        {
            smoothedFreq[bi] = fT;
            smoothedGain[bi] = gT;
            smoothedQLog[bi] = qT;
            types[bi] = wantedType;
        }
        else
        {
            smoothedFreq[bi] += a * (fT - smoothedFreq[bi]);
            smoothedGain[bi] += a * (gT - smoothedGain[bi]);
            smoothedQLog[bi] += a * (qT - smoothedQLog[bi]);
        }

        // IN de banda y cambio de tipo: baja a 0, cambia y vuelve a subir.
        amountPrev[bi] = amountNow[bi];
        const bool typeChange = wantedType != types[bi];
        const float target = (p.on && ! typeChange) ? 1.0f : 0.0f;
        if (first)
            amountNow[bi] = target;
        else
            amountNow[bi] = amountNow[bi] < target ? std::min (target, amountNow[bi] + step)
                                                   : std::max (target, amountNow[bi] - step);
        if (first)
            amountPrev[bi] = amountNow[bi];

        if (typeChange && amountNow[bi] == 0.0f && amountPrev[bi] == 0.0f)
        {
            types[bi] = wantedType;
            for (auto& ch : state)
                ch[bi].reset();
        }

        const double baseHz = std::exp (smoothedFreq[bi]);
        const double m = (mult != nullptr && std::isfinite (mult[b])) ? mult[b] : 1.0;
        effectiveFreq[bi] = clampEqFreq (baseHz * m, fs);
        smoothedFreq[bi] = std::log (clampEqFreq (baseHz, fs));
        coeffs[bi] = designSvf (types[bi], effectiveFreq[bi], smoothedGain[bi], std::exp (smoothedQLog[bi]), fs);
    }
    first = false;
}

void EchoEq::process (float* left, float* right, int n) noexcept
{
    if (globalNow == 0.0f && globalPrev == 0.0f)
    {
        for (auto& ch : state)
            for (auto& s : ch)
                s.reset();
        return;
    }

    float* io[2] = { left, right };
    const float invN = 1.0f / (float) std::max (1, n);

    // EQ IN global: fundido entre la señal sin ecualizar y la ecualizada.
    const bool globalBlend = ! (globalPrev == 1.0f && globalNow == 1.0f);
    float dry[2][kMaxEqBlock];
    if (globalBlend)
        for (int ch = 0; ch < channels; ++ch)
            if (io[ch] != nullptr)
                std::copy (io[ch], io[ch] + n, dry[ch]);

    for (int b = 0; b < kNumEqBands; ++b)
    {
        const auto bi = (size_t) b;
        const float a0 = amountPrev[bi], a1 = amountNow[bi];
        if (a0 == 0.0f && a1 == 0.0f)
        {
            for (auto& ch : state)
                ch[bi].reset();
            continue;
        }
        const auto& c = coeffs[bi];
        for (int ch = 0; ch < channels; ++ch)
        {
            float* x = io[ch];
            if (x == nullptr)
                continue;
            auto& s = state[(size_t) ch][bi];
            if (a0 == 1.0f && a1 == 1.0f)
            {
                for (int i = 0; i < n; ++i)
                    x[i] = (float) s.process (c, (double) x[i]);
            }
            else
            {
                for (int i = 0; i < n; ++i)
                {
                    const float amt = a0 + (a1 - a0) * (float) (i + 1) * invN;
                    const float y = (float) s.process (c, (double) x[i]);
                    x[i] += amt * (y - x[i]);
                }
            }
        }
    }

    if (globalBlend)
        for (int ch = 0; ch < channels; ++ch)
            if (float* x = io[ch])
                for (int i = 0; i < n; ++i)
                {
                    const float g = globalPrev + (globalNow - globalPrev) * (float) (i + 1) * invN;
                    x[i] = dry[ch][i] + g * (x[i] - dry[ch][i]);
                }
}

} // namespace labd
