#include "plugin/PluginProcessor.h"

#include "ui/PluginEditor.h"

#include <charconv>

namespace labd {

namespace {
const juce::Identifier kStateVersionId ("labd_state_version");
const juce::Identifier kSelectedBandId ("ui_selected_band");
const juce::Identifier kEditorWidthId ("ui_editor_width");

int limitEditorWidth (int width) noexcept
{
    return juce::jlimit (LabdEditor::kMinWidth, ui::kDesignWidth, width);
}

int editorWidthFromState (const juce::var& value)
{
    // El XML de JUCE puede devolver el valor como texto. Exige un entero positivo completo.
    const auto text = value.toString().trim();
    const auto* begin = text.toRawUTF8();
    const auto* end = begin + text.getNumBytesAsUTF8();
    int width = 0;
    const auto result = std::from_chars (begin, end, width);
    return result.ec == std::errc{} && result.ptr == end && width > 0
               ? limitEditorWidth (width) : LabdEditor::kNominalWidth;
}
} // namespace

LabdProcessor::LabdProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LABD", createParameterLayout()),
      refs (apvts),
      editorWidth (LabdEditor::kNominalWidth)
{
    apvts.state.setProperty (kStateVersionId, kStateVersion, nullptr);
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
    lastBlockEndMs = 0.0;
    lastBlockDurationMs = 0.0;
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

    // Reinicio explícito del host, aplicado antes de grabar la entrada nueva.
    // Si el host venía procesando sin pausa, la cola anterior se funde en 10 ms;
    // tras una pausa (stop, setProcessing(false), inicio de un bounce) es un
    // reinicio completo y la cola anterior no vuelve a sonar.
    const double blockStartMs = juce::Time::getMillisecondCounterHiRes();
    if (resetRequested.exchange (false))
    {
        const double gap = blockStartMs - lastBlockEndMs;
        const bool continuous = lastBlockEndMs > 0.0 && gap < juce::jmax (10.0, 4.0 * lastBlockDurationMs);
        engine.hostReset (continuous);
    }

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
    lastBlockEndMs = juce::Time::getMillisecondCounterHiRes();
    lastBlockDurationMs = 1000.0 * n / juce::jmax (1.0, currentSampleRate);
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
}

double LabdProcessor::tailSecondsFor (const EngineParams& p, double bpm)
{
    // Tiempo mayor x repeticiones hasta -60 dB, con margen para la saturación y
    // las transiciones; acotado a 60 s.
    double msL = 0.0, msR = 0.0;
    DelayEngine::effectiveTimesMs (p, bpm, msL, msR);
    const double longest = juce::jmax (msL, msR) / 1000.0;
    const double fb = juce::jlimit (0.0, 0.98, (double) p.feedback);
    const double repeats = fb > 1.0e-3 ? std::ceil (std::log (1.0e-3) / std::log (fb)) : 1.0;
    return juce::jlimit (0.1, 60.0, longest * (repeats + 1.0) + 0.1);
}

double LabdProcessor::getTailLengthSeconds() const
{
    // Se calcula al pedirla (también antes del primer bloque, tras cargar un estado).
    const double bpm = juce::jlimit (tempo::kMinBpm, tempo::kMaxBpm, (double) telemetry.bpm.load());
    return tailSecondsFor (refs.read(), bpm);
}

juce::AudioProcessorParameter* LabdProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

int LabdProcessor::getSelectedBand() const
{
    return selectedBand.load();
}

void LabdProcessor::setSelectedBand (int band)
{
    selectedBand.store (juce::jlimit (0, kNumEqBands - 1, band));
}

void LabdProcessor::setEditorWidth (int width)
{
    width = limitEditorWidth (width);
    if (editorWidth.exchange (width, std::memory_order_relaxed) != width)
        updateHostDisplay (juce::AudioProcessorListener::ChangeDetails{}.withNonParameterStateChanged (true));
}

void LabdProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kStateVersionId, kStateVersion, nullptr);
    state.setProperty (kSelectedBandId, selectedBand.load(), nullptr);
    state.setProperty (kEditorWidthId, getEditorWidth(), nullptr);
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
            setSelectedBand ((int) tree.getProperty (kSelectedBandId, 2));
            const auto width = editorWidthFromState (tree.getProperty (kEditorWidthId));
            tree.removeProperty (kEditorWidthId, nullptr);
            apvts.replaceState (tree);
            editorWidth.store (width, std::memory_order_relaxed);
            editHistory.requestClear(); // el UndoManager solo se toca en el hilo de mensajes
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
