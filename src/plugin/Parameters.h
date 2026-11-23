// VitaPlug LAB-D · parámetros del DAW (wrapper JUCE).
// IDs estables (versión 1). No renombrar: las sesiones guardadas dependen de ellos.
#pragma once

#include "engine/DelayEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace labd::ids {
inline constexpr auto timeSync = "time_sync";
inline constexpr auto timeLink = "time_link";
inline constexpr auto timeLMs = "time_l_ms";
inline constexpr auto timeRMs = "time_r_ms";
inline constexpr auto timeLNote = "time_l_note";
inline constexpr auto timeRNote = "time_r_note";
inline constexpr auto feedback = "feedback";
inline constexpr auto stereoMode = "stereo_mode";
inline constexpr auto color = "color";
inline constexpr auto drive = "drive";
inline constexpr auto duck = "duck";
inline constexpr auto mix = "mix";
inline constexpr auto wetOnly = "wet_only";
inline constexpr auto output = "output";
inline constexpr auto bypass = "bypass";
inline constexpr auto pan = "pan";
inline constexpr auto eqOn = "eq_on";
inline constexpr auto eq6Type = "eq6_type";
inline constexpr auto lfoOn = "lfo_on";
inline constexpr auto lfoMode = "lfo_mode";
inline constexpr auto lfoShape = "lfo_shape";
inline constexpr auto lfoRate = "lfo_rate";
inline constexpr auto lfoRateSync = "lfo_rate_sync";
inline constexpr auto lfoRateNote = "lfo_rate_note";
inline constexpr auto lfoPhase = "lfo_phase";

juce::String eqOnId (int band);   // "eq1_on"..."eq6_on"   (band 0..5)
juce::String eqFreqId (int band); // "eq1_freq"
juce::String eqGainId (int band); // "eq2_gain".. (bandas 2-6)
juce::String eqQId (int band);    // "eq1_q", "eq3_q".."eq6_q"
juce::String lfoDepthId (int index); // "lfo_depth_1".."lfo_depth_6", "lfo_depth_pan"

bool bandHasGain (int band);
bool bandHasQ (int band);
} // namespace labd::ids

namespace labd {

constexpr int kParameterVersion = 1;

// Valores INIT (ver docs/PARAMETROS.md).
struct InitBand
{
    double freq, gain, q;
    bool on;
};
extern const InitBand kInitBands[kNumEqBands];

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Punteros a los valores crudos, resueltos una vez (lectura sin bloqueo).
struct ParameterRefs
{
    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

    std::atomic<float>* raw (const juce::String& id) const;

    std::atomic<float>*timeSync, *timeLink, *timeLMs, *timeRMs, *timeLNote, *timeRNote;
    std::atomic<float>*feedback, *stereoMode, *color, *drive, *duck, *mix, *wetOnly, *output, *bypass, *pan;
    std::atomic<float>*eqOn, *eq6Type;
    std::array<std::atomic<float>*, kNumEqBands> eqBandOn {}, eqFreq {}, eqGain {}, eqQ {};
    std::atomic<float>*lfoOn, *lfoMode, *lfoShape, *lfoRate, *lfoRateSync, *lfoRateNote, *lfoPhase;
    std::array<std::atomic<float>*, kNumLfoDepths> lfoDepth {};

private:
    juce::AudioProcessorValueTreeState& apvts;
};

// Comprueba si un texto empieza por un número (entrada precisa del editor).
bool isNumericText (const juce::String& text);

// Formatos de texto compartidos por el host y el editor.
namespace format {
juce::String timeMs (double ms);
juce::String frequency (double hz);
juce::String decibels (double db, int decimals = 1);
juce::String percent (double pct);
juce::String q (double q);
juce::String hz (double hz);
juce::String pan (double pan100);
} // namespace format

} // namespace labd
