#include "plugin/PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace labd {

namespace {
const juce::Identifier kStateVersionId ("labd_state_version");
const juce::Identifier kSelectedBandId ("ui_selected_band");
} // namespace

LabdProcessor::LabdProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LABD", createParameterLayout()),
      refs (apvts)
{
    apvts.state.setProperty (kStateVersionId, kStateVersion, nullptr);
    apvts.state.setProperty (kSelectedBandId, 2, nullptr);
}

bool LabdProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    // mono → mono, estéreo → estéreo y mono → estéreo.
    return (in == mono && out == mono) || (in == stereo && out == stereo) || (in == mono && out == stereo);
}

void LabdProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int outCh = getTotalNumOutputChannels();
    engine.prepare (sampleRate, samplesPerBlock, juce::jlimit (1, 2, outCh));
    engine.setParams (refs.read());
    resetRequested.store (false);
    telemetry.inputChannels.store (getTotalNumInputChannels());
    telemetry.outputChannels.store (outCh);
    setLatencySamples (0);
}

void LabdProcessor::reset()
{
    // Puede llegar desde cualquier hilo (p. ej. AU Reset): se ejecuta en el
    // siguiente bloque de audio con un fundido corto del wet.
    resetRequested.store (true);
}

void LabdProcessor::runEngine (juce::AudioBuffer<float>& buffer, bool forceBypass)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = juce::jmin (2, getTotalNumOutputChannels(), buffer.getNumChannels());
    if (n <= 0 || outCh <= 0)
        return;

    // mono → estéreo: la entrada mono alimenta los dos canales del motor.
    if (inCh == 1 && outCh == 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    for (int c = outCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (resetRequested.exchange (false))
        engine.beginSoftReset();

    auto params = refs.read();
    if (forceBypass)
        params.bypass = true;
    engine.setParams (params);

    TransportInfo t;
    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
        {
            if (const auto bpm = pos->getBpm())
            {
                t.hasBpm = true;
                t.bpm = *bpm;
            }
            if (const auto ppq = pos->getPpqPosition())
            {
                t.hasPpq = true;
                t.ppq = *ppq;
            }
            if (const auto sig = pos->getTimeSignature())
            {
                t.hasTimeSig = true;
                t.sigNumerator = sig->numerator;
                t.sigDenominator = sig->denominator;
            }
            t.isPlaying = pos->getIsPlaying();
        }
    }

    engine.process (buffer.getArrayOfWritePointers(), outCh, n, t);
    telemetry.inputChannels.store (inCh, std::memory_order_relaxed);
    telemetry.outputChannels.store (outCh, std::memory_order_relaxed);
    publishTelemetry();
}

void LabdProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    runEngine (buffer, false);
}

void LabdProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Bypass del host sin parámetro: el motor sigue en marcha y entrega la
    // señal seca con la misma transición suave.
    runEngine (buffer, true);
}

void LabdProcessor::publishTelemetry() noexcept
{
    const auto& tl = engine.telemetry();
    constexpr auto r = std::memory_order_relaxed;
    telemetry.timeMs[0].store ((float) tl.effectiveTimeMs[0], r);
    telemetry.timeMs[1].store ((float) tl.effectiveTimeMs[1], r);
    telemetry.bpm.store ((float) tl.bpm, r);
    telemetry.bpmFromHost.store (tl.bpmFromHost, r);
    telemetry.sigNumerator.store (tl.sigNumerator, r);
    telemetry.sigDenominator.store (tl.sigDenominator, r);
    telemetry.lfoHz.store ((float) tl.lfoHz, r);
    telemetry.lfoPhase.store ((float) tl.lfoPhase, r);
    telemetry.lfoValue.store (tl.lfoValue, r);
    telemetry.lfoGate.store (tl.lfoGate, r);
    telemetry.autoScale.store (tl.autoScale, r);
    telemetry.effectivePan.store (tl.effectivePan, r);
    for (int b = 0; b < kNumEqBands; ++b)
    {
        telemetry.eqEffectiveFreq[(size_t) b].store ((float) tl.eqEffectiveFreq[(size_t) b], r);
        telemetry.eqBandAmount[(size_t) b].store (tl.eqBandAmount[(size_t) b], r);
    }
    for (int i = 0; i < kNumLfoDepths; ++i)
        telemetry.effectiveDepth[(size_t) i].store (tl.effectiveDepth[(size_t) i], r);
    telemetry.eqGlobalAmount.store (tl.eqGlobalAmount, r);
    telemetry.duckGainDb.store (tl.duckGainDb, r);
    telemetry.effectiveMix.store (tl.effectiveMix, r);
    telemetry.blocksProcessed.fetch_add (1, r);

    // Cola: tiempo mayor x repeticiones hasta -60 dB (máximo 60 s).
    const double fb = juce::jlimit (0.0, 0.98, (double) refs.feedback->load (r) * 0.01);
    const double longest = juce::jmax (tl.effectiveTimeMs[0], tl.effectiveTimeMs[1]) / 1000.0;
    const double repeats = fb > 1.0e-3 ? std::ceil (std::log (1.0e-3) / std::log (fb)) : 1.0;
    tailSeconds.store ((float) juce::jmin (60.0, longest * (repeats + 1.0)), r);
}

double LabdProcessor::getTailLengthSeconds() const
{
    return (double) tailSeconds.load();
}

juce::AudioProcessorParameter* LabdProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

int LabdProcessor::getSelectedBand() const
{
    return juce::jlimit (0, kNumEqBands - 1, (int) apvts.state.getProperty (kSelectedBandId, 2));
}

void LabdProcessor::setSelectedBand (int band)
{
    apvts.state.setProperty (kSelectedBandId, juce::jlimit (0, kNumEqBands - 1, band), nullptr);
}

void LabdProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kStateVersionId, kStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void LabdProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            if (! tree.hasProperty (kSelectedBandId))
                tree.setProperty (kSelectedBandId, 2, nullptr);
            apvts.replaceState (tree);
            undoManager.clearUndoHistory();
        }
    }
}

juce::AudioProcessorEditor* LabdProcessor::createEditor()
{
    return new LabdEditor (*this);
}

} // namespace labd

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new labd::LabdProcessor();
}
