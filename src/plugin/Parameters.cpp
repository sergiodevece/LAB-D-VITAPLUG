#include "plugin/Parameters.h"

namespace labd::ids {
juce::String eqOnId (int b) { return "eq" + juce::String (b + 1) + "_on"; }
juce::String eqFreqId (int b) { return "eq" + juce::String (b + 1) + "_freq"; }
juce::String eqGainId (int b) { return "eq" + juce::String (b + 1) + "_gain"; }
juce::String eqQId (int b) { return "eq" + juce::String (b + 1) + "_q"; }
juce::String lfoDepthId (int i) { return i == kPanDepthIndex ? juce::String ("lfo_depth_pan") : "lfo_depth_" + juce::String (i + 1); }
bool bandHasGain (int b) { return b != 0; }   // LOW CUT no tiene ganancia
bool bandHasQ (int b) { return b != 1; }      // LOW SHELF tiene pendiente fija
} // namespace labd::ids

namespace labd {

const InitBand kInitBands[kNumEqBands] = {
    { 30.0, 0.0, 0.707, false },   // 1 LOW CUT (fuera)
    { 120.0, 0.0, 0.707, true },   // 2 LOW SHELF 0 dB
    { 400.0, 0.0, 1.0, true },     // 3 BELL 0 dB
    { 1500.0, 0.0, 1.0, true },    // 4 BELL 0 dB
    { 5000.0, 0.0, 1.0, true },    // 5 BELL 0 dB
    { 12000.0, 0.0, 0.707, false } // 6 HIGH CUT (fuera)
};

namespace format {
juce::String timeMs (double ms)
{
    if (ms >= 1000.0)
        return juce::String (ms, 0) + " ms";
    if (ms >= 100.0)
        return juce::String (ms, 0) + " ms";
    return juce::String (ms, 1) + " ms";
}

juce::String frequency (double hz)
{
    if (hz >= 1000.0)
        return juce::String (hz / 1000.0, hz >= 10000.0 ? 1 : 2) + " kHz";
    return juce::String (juce::roundToInt (hz)) + " Hz";
}

juce::String decibels (double db, int decimals)
{
    if (std::abs (db) < 0.5 * std::pow (10.0, -decimals))
        db = 0.0;
    juce::String s = juce::String (std::abs (db), decimals);
    if (db > 0.0)
        s = "+" + s;
    else if (db < 0.0)
        s = juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + s; // signo menos
    return s + " dB";
}

juce::String percent (double pct) { return juce::String (juce::roundToInt (pct)) + "%"; }
juce::String q (double v) { return juce::String (v, 2); }
juce::String hz (double v) { return juce::String (v, v < 10.0 ? 2 : 1) + " Hz"; }

juce::String pan (double p)
{
    const int v = juce::roundToInt (p);
    if (v == 0)
        return "C";
    return (v < 0 ? "L" : "R") + juce::String (std::abs (v));
}
} // namespace format

namespace {
using Range = juce::NormalisableRange<float>;

Range logRange (float lo, float hi)
{
    return Range (lo, hi,
                  [] (float a, float b, float x) { return a * std::pow (b / a, x); },
                  [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                  [] (float a, float b, float v) { return juce::jlimit (a, b, v); });
}

float parseNumber (const juce::String& text)
{
    auto t = text.trim().toLowerCase().replace (juce::CharPointer_UTF8 ("\xe2\x88\x92"), "-").replace (",", ".");
    float mult = 1.0f;
    if (t.contains ("k"))
        mult = 1000.0f;
    return t.retainCharacters ("0123456789.-+").getFloatValue() * mult;
}

juce::StringArray noteLabels()
{
    juce::StringArray s;
    for (const auto& n : tempo::kNotes)
        s.add (n.label);
    return s;
}

juce::StringArray rateLabels()
{
    juce::StringArray s;
    for (const auto& r : tempo::kRates)
        s.add (r.label);
    return s;
}

std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id, const juce::String& name, Range range,
                                                       float def, const juce::String& label,
                                                       std::function<juce::String (float)> toText,
                                                       std::function<float (const juce::String&)> fromText = nullptr)
{
    auto attrs = juce::AudioParameterFloatAttributes()
                     .withLabel (label)
                     .withStringFromValueFunction ([toText] (float v, int) { return toText (v); });
    attrs = attrs.withValueFromStringFunction (fromText ? fromText : parseNumber);
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, kParameterVersion }, name, range, def, attrs);
}

std::unique_ptr<juce::AudioParameterBool> boolParam (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { id, kParameterVersion }, name, def,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction ([] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); })
            .withValueFromStringFunction ([] (const juce::String& t) {
                const auto s = t.trim().toLowerCase();
                return s == "on" || s == "1" || s == "true" || s == "yes";
            }));
}

std::unique_ptr<juce::AudioParameterChoice> choiceParam (const juce::String& id, const juce::String& name,
                                                         const juce::StringArray& choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, kParameterVersion }, name, choices, def);
}

juce::String numberOnly (double v, int decimals) { return juce::String (v, decimals); }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ids;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto msText = [] (float v) { return numberOnly (v, v < 100.0f ? 1 : 0); };
    auto pctText = [] (float v) { return numberOnly (v, 0); };
    auto dbText = [] (float v) { return numberOnly (std::abs (v) < 0.05f ? 0.0f : v, 1); };
    auto hzText = [] (float v) { return numberOnly (v, v < 100.0f ? 1 : 0); };

    // --- Tiempo -----------------------------------------------------------------
    layout.add (boolParam (timeSync, "Time Sync", false));
    layout.add (boolParam (timeLink, "Time Link", false));
    layout.add (floatParam (timeLMs, "Time L", logRange (1.0f, 4000.0f), 350.0f, "ms", msText));
    layout.add (floatParam (timeRMs, "Time R", logRange (1.0f, 4000.0f), 500.0f, "ms", msText));
    layout.add (choiceParam (timeLNote, "Time L Note", noteLabels(), tempo::noteIndex ("1/8D")));
    layout.add (choiceParam (timeRNote, "Time R Note", noteLabels(), tempo::noteIndex ("1/4")));

    // --- Bucle y salida -----------------------------------------------------------
    layout.add (floatParam (feedback, "Feedback", Range (0.0f, 98.0f), 30.0f, "%", pctText));
    layout.add (choiceParam (stereoMode, "Stereo Mode", { "Stereo", "Ping Pong" }, 0));
    layout.add (choiceParam (color, "Color", { "Clean", "Tape" }, 0));
    layout.add (floatParam (drive, "Drive", Range (0.0f, 100.0f), 0.0f, "%", pctText));
    layout.add (floatParam (duck, "Duck", Range (0.0f, 100.0f), 0.0f, "%", pctText));
    layout.add (floatParam (mix, "Mix", Range (0.0f, 100.0f), 25.0f, "%", pctText));
    layout.add (boolParam (wetOnly, "Wet Only", false));
    layout.add (floatParam (output, "Output", Range (-24.0f, 12.0f), 0.0f, "dB", dbText));
    layout.add (boolParam (bypass, "Bypass", false));
    layout.add (floatParam (pan, "Echo Pan", Range (-100.0f, 100.0f), 0.0f, "",
                            [] (float v) { return format::pan (v); },
                            [] (const juce::String& t) {
                                const auto s = t.trim().toUpperCase();
                                const float v = s.retainCharacters ("0123456789.").getFloatValue();
                                return s.startsWith ("L") ? -v : (s.startsWith ("R") ? v : (s.startsWith ("C") ? 0.0f : s.getFloatValue()));
                            }));

    // --- ECHO EQ --------------------------------------------------------------------
    layout.add (boolParam (eqOn, "EQ In", true));
    const char* bandNames[kNumEqBands] = { "Low Cut", "Low Shelf", "Bell 3", "Bell 4", "Bell 5", "High" };
    for (int b = 0; b < kNumEqBands; ++b)
    {
        const auto prefix = "EQ " + juce::String (b + 1) + " " + bandNames[b];
        layout.add (boolParam (eqOnId (b), prefix + " In", kInitBands[b].on));
        layout.add (floatParam (eqFreqId (b), prefix + " Freq", logRange (20.0f, 20000.0f), (float) kInitBands[b].freq, "Hz", hzText));
        if (bandHasGain (b))
            layout.add (floatParam (eqGainId (b), prefix + " Gain", Range (-18.0f, 18.0f), (float) kInitBands[b].gain, "dB", dbText));
        if (bandHasQ (b))
            layout.add (floatParam (eqQId (b), prefix + " Q", logRange (0.25f, 8.0f), (float) kInitBands[b].q, "",
                                    [] (float v) { return format::q (v); }));
    }
    layout.add (choiceParam (eq6Type, "EQ 6 Type", { "High Cut", "High Shelf" }, 0));

    // --- LFO --------------------------------------------------------------------------
    layout.add (boolParam (lfoOn, "LFO On", false));
    layout.add (choiceParam (lfoMode, "LFO Mode", { "Manual", "Auto" }, 0));
    layout.add (choiceParam (lfoShape, "LFO Shape", { "Sine", "Triangle", "Saw", "Square", "Random" }, 0));
    layout.add (floatParam (lfoRate, "LFO Rate", logRange (0.01f, 20.0f), 1.0f, "Hz",
                            [] (float v) { return numberOnly (v, v < 10.0f ? 2 : 1); }));
    layout.add (boolParam (lfoRateSync, "LFO Rate Sync", false));
    layout.add (choiceParam (lfoRateNote, "LFO Rate Note", rateLabels(), tempo::rateIndex ("1/2")));
    layout.add (floatParam (lfoPhase, "LFO Phase", Range (0.0f, 360.0f, 1.0f), 0.0f, juce::CharPointer_UTF8 ("\xc2\xb0"),
                            [] (float v) { return numberOnly (v, 0); }));
    for (int i = 0; i < kNumLfoDepths; ++i)
        layout.add (floatParam (lfoDepthId (i), i == kPanDepthIndex ? juce::String ("LFO Depth Pan") : "LFO Depth " + juce::String (i + 1),
                                Range (0.0f, 100.0f), 0.0f, "%", pctText));
    return layout;
}

ParameterRefs::ParameterRefs (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    using namespace ids;
    timeSync = raw (ids::timeSync);
    timeLink = raw (ids::timeLink);
    timeLMs = raw (ids::timeLMs);
    timeRMs = raw (ids::timeRMs);
    timeLNote = raw (ids::timeLNote);
    timeRNote = raw (ids::timeRNote);
    feedback = raw (ids::feedback);
    stereoMode = raw (ids::stereoMode);
    color = raw (ids::color);
    drive = raw (ids::drive);
    duck = raw (ids::duck);
    mix = raw (ids::mix);
    wetOnly = raw (ids::wetOnly);
    output = raw (ids::output);
    bypass = raw (ids::bypass);
    pan = raw (ids::pan);
    eqOn = raw (ids::eqOn);
    eq6Type = raw (ids::eq6Type);
    for (int b = 0; b < kNumEqBands; ++b)
    {
        eqBandOn[(size_t) b] = raw (eqOnId (b));
        eqFreq[(size_t) b] = raw (eqFreqId (b));
        eqGain[(size_t) b] = bandHasGain (b) ? raw (eqGainId (b)) : nullptr;
        eqQ[(size_t) b] = bandHasQ (b) ? raw (eqQId (b)) : nullptr;
    }
    lfoOn = raw (ids::lfoOn);
    lfoMode = raw (ids::lfoMode);
    lfoShape = raw (ids::lfoShape);
    lfoRate = raw (ids::lfoRate);
    lfoRateSync = raw (ids::lfoRateSync);
    lfoRateNote = raw (ids::lfoRateNote);
    lfoPhase = raw (ids::lfoPhase);
    for (int i = 0; i < kNumLfoDepths; ++i)
        lfoDepth[(size_t) i] = raw (lfoDepthId (i));
}

std::atomic<float>* ParameterRefs::raw (const juce::String& id) const
{
    auto* p = apvts.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

EngineParams ParameterRefs::read() const noexcept
{
    auto f = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    auto b = [&] (const std::atomic<float>* a) { return f (a) >= 0.5f; };
    auto i = [&] (const std::atomic<float>* a) { return juce::roundToInt (f (a)); };
    // Porcentajes: división exacta (0 y 100 dan 0 y 1 exactos).
    auto pct = [&] (const std::atomic<float>* a) { return f (a) / 100.0f; };
    // La normalización logarítmica del host no devuelve el valor exacto
    // (100 ms → 99,9999 ms; 0 dB → 3e-6 dB). Se cuantifica a 0,001 ms y
    // 0,001 dB: tiempos enteros en muestras y ganancia unitaria exacta.
    auto quant = [] (double v, double step) { return std::round (v / step) * step; };

    EngineParams p;
    p.sync = b (timeSync);
    p.link = b (timeLink);
    p.timeMsL = quant (f (timeLMs), 0.001);
    p.timeMsR = quant (f (timeRMs), 0.001);
    p.noteL = i (timeLNote);
    p.noteR = i (timeRNote);
    p.feedback = pct (feedback);
    p.pingPong = i (stereoMode) == 1;
    p.tape = i (color) == 1;
    p.drive = pct (drive);
    p.duck = pct (duck);
    p.mix = pct (mix);
    p.wetOnly = b (wetOnly);
    p.outputDb = (float) quant (f (output), 0.001);
    p.bypass = b (bypass);
    p.pan = pct (pan);
    p.eq.eqOn = b (eqOn);
    for (int k = 0; k < kNumEqBands; ++k)
    {
        auto& band = p.eq.band[(size_t) k];
        band.on = b (eqBandOn[(size_t) k]);
        band.freq = f (eqFreq[(size_t) k]);
        band.gainDb = eqGain[(size_t) k] != nullptr ? f (eqGain[(size_t) k]) : 0.0;
        band.q = eqQ[(size_t) k] != nullptr ? f (eqQ[(size_t) k]) : kShelfQ;
        band.highShelf = (k == 5) && i (eq6Type) == 1;
    }
    p.lfoOn = b (lfoOn);
    p.lfoAuto = i (lfoMode) == 1;
    p.shape = (LfoShape) juce::jlimit (0, 4, i (lfoShape));
    p.rateHz = f (lfoRate);
    p.rateSync = b (lfoRateSync);
    p.rateIndex = i (lfoRateNote);
    p.phaseDeg = f (lfoPhase);
    for (int k = 0; k < kNumLfoDepths; ++k)
        p.depth[(size_t) k] = pct (lfoDepth[(size_t) k]);
    return p;
}

} // namespace labd
