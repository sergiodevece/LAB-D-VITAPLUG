// VitaPlug LAB-D · wrapper JUCE (AU v2 / VST3).
#pragma once

#include "engine/DelayEngine.h"
#include "plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace labd {

// Instantánea de tiempo real publicada hacia el editor (sin bloqueos).
struct UiTelemetry
{
    std::atomic<float> timeMs[2] { 350.0f, 500.0f };
    std::atomic<float> bpm { 120.0f };
    std::atomic<bool> bpmFromHost { false };
    std::atomic<int> sigNumerator { 4 }, sigDenominator { 4 };
    std::atomic<int> inputChannels { 2 }, outputChannels { 2 };
    std::atomic<float> lfoHz { 1.0f }, lfoPhase { 0.0f }, lfoValue { 0.0f }, lfoGate { 0.0f }, autoScale { 1.0f };
    std::atomic<float> effectivePan { 0.0f };
    std::array<std::atomic<float>, kNumEqBands> eqEffectiveFreq {}, eqBandAmount {};
    std::array<std::atomic<float>, kNumLfoDepths> effectiveDepth {};
    std::atomic<float> eqGlobalAmount { 1.0f };
    std::atomic<float> duckGainDb { 0.0f };
    std::atomic<float> effectiveMix { 0.25f };
    std::atomic<int> blocksProcessed { 0 };
};

class LabdProcessor final : public juce::AudioProcessor {
public:
    LabdProcessor();
    ~LabdProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LAB-D"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "INIT"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    const UiTelemetry& getTelemetry() const noexcept { return telemetry; }
    const DelayEngine& getEngine() const noexcept { return engine; }

    // Estado del editor guardado con la sesión (banda seleccionada).
    int getSelectedBand() const;
    void setSelectedBand (int band);

    static constexpr int kStateVersion = 1;

private:
    void runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass);
    void publishTelemetry() noexcept;

    juce::UndoManager undoManager { 30000, 60 };
    juce::AudioProcessorValueTreeState apvts;
    ParameterRefs refs;
    DelayEngine engine;
    UiTelemetry telemetry;
    std::atomic<bool> resetRequested { false };
    std::atomic<float> tailSeconds { 2.0f };
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabdProcessor)
};

} // namespace labd
