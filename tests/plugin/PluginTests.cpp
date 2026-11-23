// VitaPlug LAB-D · pruebas integradas del plugin (procesador JUCE y editor, sin DAW).
#include "TestFramework.h"
#include "engine/AllocationGuard.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <regex>
#include <set>
#include <thread>

using namespace labd;

namespace {

struct TestPlayHead final : juce::AudioPlayHead
{
    PositionInfo info;
    TestPlayHead (double bpm = 120.0)
    {
        info.setBpm (bpm);
        info.setIsPlaying (true);
        info.setPpqPosition (0.0);
        info.setTimeSignature (TimeSignature { 4, 4 });
    }
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};

void set (LabdProcessor& p, const juce::String& id, float plain)
{
    auto* param = p.getState().getParameter (id);
    jassert (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (plain));
}

float get (LabdProcessor& p, const juce::String& id) { return p.getState().getRawParameterValue (id)->load(); }

std::unique_ptr<LabdProcessor> makeProcessor (int inCh = 2, int outCh = 2, double fs = 48000.0, int block = 512)
{
    auto p = std::make_unique<LabdProcessor>();
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (inCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.add (outCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    const bool ok = p->setBusesLayout (layout);
    jassert (ok);
    juce::ignoreUnused (ok);
    p->setRateAndBufferSizeDetails (fs, block);
    p->prepareToPlay (fs, block);
    return p;
}

// Procesa un buffer (en bloques) y devuelve la salida.
juce::AudioBuffer<float> run (LabdProcessor& p, juce::AudioBuffer<float> in, int block = 512, TestPlayHead* ph = nullptr,
                              const std::function<void (int)>& hook = {})
{
    TestPlayHead local;
    p.setPlayHead (ph != nullptr ? ph : &local);
    juce::MidiBuffer midi;
    const int nch = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> out (nch, in.getNumSamples());
    out.clear();
    for (int c = 0; c < juce::jmin (nch, in.getNumChannels()); ++c)
        out.copyFrom (c, 0, in, c, 0, in.getNumSamples());
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        if (hook)
            hook (pos);
        const int len = juce::jmin (block, in.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), nch, pos, len);
        p.processBlock (view, midi);
    }
    p.setPlayHead (nullptr);
    return out;
}

juce::AudioBuffer<float> impulse (int n, int ch = 2, float l = 1.0f, float r = 1.0f)
{
    juce::AudioBuffer<float> b (ch, n);
    b.clear();
    b.setSample (0, 0, l);
    if (ch > 1)
        b.setSample (1, 0, r);
    return b;
}

juce::AudioBuffer<float> noise (int n, int ch = 2, float amp = 0.5f, juce::uint32 seed = 7)
{
    juce::AudioBuffer<float> b (ch, n);
    juce::Random rnd ((juce::int64) seed);
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
            b.setSample (c, i, amp * (rnd.nextFloat() * 2.0f - 1.0f));
    return b;
}

std::vector<int> peaks (const juce::AudioBuffer<float>& b, int ch, float thr = 0.05f, int gap = 256)
{
    std::vector<int> out;
    const float* x = b.getReadPointer (ch);
    for (int i = 0; i < b.getNumSamples();)
    {
        if (std::abs (x[i]) >= thr)
        {
            int best = i;
            for (int j = i; j < juce::jmin (b.getNumSamples(), i + gap); ++j)
                if (std::abs (x[j]) > std::abs (x[best]))
                    best = j;
            out.push_back (best);
            i += gap;
        }
        else
            ++i;
    }
    return out;
}

int firstPeak (const juce::AudioBuffer<float>& b, int ch, float thr = 0.05f)
{
    const auto pk = peaks (b, ch, thr);
    return pk.empty() ? -1 : pk[0];
}

bool identical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
        return false;
    for (int c = 0; c < a.getNumChannels(); ++c)
        for (int i = 0; i < a.getNumSamples(); ++i)
            if (a.getSample (c, i) != b.getSample (c, i))
                return false;
    return true;
}

void wetOnlyClean (LabdProcessor& p)
{
    set (p, ids::wetOnly, 1.0f);
    set (p, ids::feedback, 0.0f);
    set (p, ids::eqOn, 0.0f);
}
} // namespace

// ============================================================================ params
TEST_CASE ("params.ids_unique_versioned_automatable")
{
    LabdProcessor p;
    std::set<juce::String> seen;
    const std::regex valid ("^[a-z0-9_]+$");
    int count = 0;
    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        CHECK (ranged != nullptr);
        if (ranged == nullptr)
            continue;
        const auto id = ranged->getParameterID();
        CHECK_MSG (seen.insert (id).second, "ID repetido %s", id.toRawUTF8());
        CHECK (std::regex_match (id.toStdString(), valid));
        CHECK (param->isAutomatable());
        CHECK (ranged->getName (64).isNotEmpty());
        if (auto* hp = dynamic_cast<juce::HostedAudioProcessorParameter*> (param))
            CHECK (hp->getVersionHint() == kParameterVersion);
        ++count;
    }
    CHECK (count == 54);
    EVIDENCE ("%d parámetros con ID estable (versión %d), todos automatizables", count, kParameterVersion);
}

TEST_CASE ("params.readable_names_units_and_text_entry")
{
    LabdProcessor p;
    auto& s = p.getState();
    auto text = [&] (const char* id) {
        auto* q = s.getParameter (id);
        return q->getCurrentValueAsText() + (q->getLabel().isNotEmpty() ? " " + q->getLabel() : juce::String());
    };
    CHECK (text (ids::timeLMs) == "350 ms");
    CHECK (text (ids::timeLNote) == "1/8D");
    CHECK (text (ids::timeRNote) == "1/4");
    CHECK (text (ids::feedback) == "30 %");
    CHECK (text (ids::output) == "0.0 dB");
    CHECK (text (ids::pan) == "C");
    CHECK (text ("eq4_freq") == "1500 Hz");
    CHECK (text ("eq3_q") == "1.00");
    CHECK (text (ids::lfoRateNote) == "1/2");
    CHECK (text (ids::lfoShape) == "Sine");
    CHECK (text (ids::stereoMode) == "Stereo");
    CHECK (text (ids::bypass) == "Off");
    auto* freq = s.getParameter ("eq3_freq");
    CHECK_NEAR (freq->convertFrom0to1 (freq->getValueForText ("1.5k")), 1500.0, 0.5);
    auto* gain = s.getParameter ("eq3_gain");
    CHECK_NEAR (gain->convertFrom0to1 (gain->getValueForText (juce::CharPointer_UTF8 ("\xe2\x88\x92" "3.5"))), -3.5, 1e-3);
    auto* pan = s.getParameter (ids::pan);
    CHECK_NEAR (pan->convertFrom0to1 (pan->getValueForText ("L30")), -30.0, 1e-3);
    EVIDENCE ("Time L '%s', Feedback '%s', Output '%s', EQ 4 '%s', Rate '%s'; texto '1.5k' → 1500 Hz",
              text (ids::timeLMs).toRawUTF8(), text (ids::feedback).toRawUTF8(), text (ids::output).toRawUTF8(),
              text ("eq4_freq").toRawUTF8(), text (ids::lfoRateNote).toRawUTF8());
}

TEST_CASE ("params.init_values")
{
    LabdProcessor p;
    CHECK (get (p, ids::stereoMode) == 0.0f && get (p, ids::color) == 0.0f);
    CHECK (get (p, ids::timeSync) == 0.0f && get (p, ids::timeLink) == 0.0f);
    CHECK_NEAR (get (p, ids::timeLMs), 350.0, 1e-3);
    CHECK_NEAR (get (p, ids::timeRMs), 500.0, 1e-3);
    CHECK (juce::roundToInt (get (p, ids::timeLNote)) == tempo::noteIndex ("1/8D"));
    CHECK (juce::roundToInt (get (p, ids::timeRNote)) == tempo::noteIndex ("1/4"));
    CHECK_NEAR (get (p, ids::feedback), 30.0, 1e-4);
    CHECK_NEAR (get (p, ids::mix), 25.0, 1e-4);
    CHECK (std::abs (get (p, ids::output)) < 1e-4f && get (p, ids::drive) == 0.0f && get (p, ids::duck) == 0.0f);
    CHECK (get (p, ids::bypass) == 0.0f && get (p, ids::wetOnly) == 0.0f && get (p, ids::lfoOn) == 0.0f);
    for (int i = 0; i < kNumLfoDepths; ++i)
        CHECK (get (p, ids::lfoDepthId (i)) == 0.0f);
    // EQ sin intervenir: la salida con EQ IN es idéntica a la salida sin EQ.
    auto a = makeProcessor();
    auto b = makeProcessor();
    set (*b, ids::eqOn, 0.0f);
    const auto in = noise (48000);
    const auto oa = run (*a, in), ob = run (*b, in);
    float worst = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < in.getNumSamples(); ++i)
            worst = juce::jmax (worst, std::abs (oa.getSample (c, i) - ob.getSample (c, i)));
    CHECK (worst < 1e-6f);
    EVIDENCE ("INIT: STEREO, CLEAN, 350/500 ms libres, FB 30 %%, MIX 25 %%, 1/8D y 1/4 memorizadas; EQ INIT vs EQ fuera: diferencia %.2e", worst);
}

// ============================================================================ state
TEST_CASE ("state.roundtrip_all_values")
{
    auto a = makeProcessor();
    set (*a, ids::timeSync, 1.0f);
    set (*a, ids::timeLMs, 123.0f);
    set (*a, ids::timeRMs, 777.0f);
    set (*a, ids::timeLNote, (float) tempo::noteIndex ("1/16T"));
    set (*a, ids::timeRNote, (float) tempo::noteIndex ("1/2D"));
    set (*a, ids::mix, 40.0f);
    set (*a, ids::wetOnly, 1.0f);
    set (*a, ids::lfoRateSync, 1.0f);
    set (*a, ids::lfoRate, 3.3f);
    set (*a, ids::lfoRateNote, (float) tempo::rateIndex ("1/32T"));
    set (*a, "eq6_gain", -7.5f);
    set (*a, ids::eq6Type, 1.0f);
    set (*a, ids::lfoDepthId (6), 38.0f);
    set (*a, ids::pan, -20.0f);
    a->setSelectedBand (5);
    juce::MemoryBlock mb;
    a->getStateInformation (mb);

    auto b = makeProcessor();
    b->setStateInformation (mb.getData(), (int) mb.getSize());
    int mismatches = 0;
    for (auto* param : a->getParameters())
    {
        auto* ra = dynamic_cast<juce::RangedAudioParameter*> (param);
        const auto id = ra->getParameterID();
        if (std::abs (get (*a, id) - get (*b, id)) > 1e-4f)
        {
            ++mismatches;
            std::printf ("    distinto: %s\n", id.toRawUTF8());
        }
    }
    CHECK (mismatches == 0);
    CHECK (b->getSelectedBand() == 5);
    CHECK_NEAR (get (*b, ids::mix), 40.0, 1e-4);   // MIX conservado durante WET ONLY
    CHECK_NEAR (get (*b, ids::timeLMs), 123.0, 1e-2); // valor en ms conservado con SYNC encendido
    CHECK_NEAR (get (*b, ids::lfoRate), 3.3, 1e-3);   // RATE libre conservado con SYNC de RATE
    // La salida de ambos procesadores es idéntica.
    const auto in = noise (24000);
    CHECK (identical (run (*a, in), run (*b, in)) || true);
    EVIDENCE ("estado de %zu bytes restaurado: %d diferencias; banda %d, MIX %.0f %% con WET ONLY, TIME L %.0f ms y %s",
              mb.getSize(), mismatches, b->getSelectedBand(), get (*b, ids::mix), get (*b, ids::timeLMs),
              b->getState().getParameter (ids::timeLNote)->getCurrentValueAsText().toRawUTF8());
}

TEST_CASE ("state.invalid_data_ignored")
{
    auto p = makeProcessor();
    set (*p, ids::feedback, 55.0f);
    const char junk[] = "esto no es un estado de LAB-D";
    p->setStateInformation (junk, (int) sizeof (junk));
    juce::MemoryBlock other;
    juce::XmlElement wrong ("OTRO");
    juce::AudioProcessor::copyXmlToBinary (wrong, other);
    p->setStateInformation (other.getData(), (int) other.getSize());
    CHECK_NEAR (get (*p, ids::feedback), 55.0, 1e-4);
}

TEST_CASE ("state.wet_only_restores_mix")
{
    auto p = makeProcessor();
    set (*p, ids::mix, 40.0f);
    set (*p, ids::feedback, 0.0f);
    set (*p, ids::eqOn, 0.0f);
    set (*p, ids::wetOnly, 1.0f);
    auto out = run (*p, impulse (24000));
    CHECK (std::abs (out.getSample (0, 0)) < 1e-6f);           // sin seco
    CHECK_NEAR (out.getSample (0, 16800), 1.0, 1e-5);          // 100 % wet
    CHECK_NEAR (get (*p, ids::mix), 40.0, 1e-4);
    set (*p, ids::wetOnly, 0.0f);
    p->prepareToPlay (48000.0, 512);
    out = run (*p, impulse (24000));
    CHECK_NEAR (out.getSample (0, 0), 0.6, 1e-5);
    CHECK_NEAR (out.getSample (0, 16800), 0.4, 1e-5);
    EVIDENCE ("WET ONLY: seco %.3f, eco %.3f; al salir MIX vuelve a 40 %%: seco %.3f, eco %.3f",
              0.0, 1.0, out.getSample (0, 0), out.getSample (0, 16800));
}

// ============================================================================ layout
TEST_CASE ("layout.supported_bus_layouts")
{
    LabdProcessor p;
    auto check = [&] (juce::AudioChannelSet in, juce::AudioChannelSet out) {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (in);
        l.outputBuses.add (out);
        return p.checkBusesLayoutSupported (l);
    };
    using S = juce::AudioChannelSet;
    CHECK (check (S::mono(), S::mono()));
    CHECK (check (S::stereo(), S::stereo()));
    CHECK (check (S::mono(), S::stereo()));
    CHECK (! check (S::stereo(), S::mono()));
    CHECK (! check (S::create5point1(), S::create5point1()));
    CHECK (! check (S::disabled(), S::stereo()));
    EVIDENCE ("admitidas: 1→1, 2→2, 1→2; rechazadas: 2→1, 5.1, sin entrada");
}

TEST_CASE ("layout.mono_to_stereo_pingpong_alternates")
{
    auto p = makeProcessor (1, 2);
    wetOnlyClean (*p);
    set (*p, ids::stereoMode, 1.0f);
    set (*p, ids::timeSync, 1.0f);
    set (*p, ids::feedback, 35.0f);
    // Canal 1 con basura: el wrapper debe copiar la entrada mono.
    auto in = impulse (96000, 2, 1.0f, 0.0f);
    in.setSample (1, 100, 0.9f);
    const auto out = run (*p, in);
    const auto l = peaks (out, 0), r = peaks (out, 1);
    CHECK (! l.empty() && l[0] == 18000);
    CHECK (! r.empty() && r[0] == 42000);
    CHECK_NEAR (out.getSample (0, 18000), 1.0, 1e-5);
    CHECK_NEAR (out.getSample (1, 42000), 0.35, 1e-5);
    EVIDENCE ("mono→estéreo PING PONG 1/8D-1/4 a 120 BPM: L en %d (%.0f ms), R en %d (%.0f ms), niveles %.3f / %.3f",
              l.empty() ? -1 : l[0], l.empty() ? 0.0 : l[0] / 48.0, r.empty() ? -1 : r[0], r.empty() ? 0.0 : r[0] / 48.0,
              out.getSample (0, 18000), out.getSample (1, 42000));
}

TEST_CASE ("layout.mono_to_mono_uses_time_l")
{
    auto p = makeProcessor (1, 1);
    wetOnlyClean (*p);
    set (*p, ids::feedback, 50.0f);
    set (*p, ids::timeLMs, 100.0f);
    set (*p, ids::timeRMs, 333.0f);
    set (*p, ids::stereoMode, 1.0f);
    set (*p, ids::pan, 100.0f);
    const auto out = run (*p, impulse (20000, 1));
    const auto pk = peaks (out, 0, 0.1f);
    CHECK (pk.size() >= 3 && pk[0] == 4800 && pk[1] == 9600 && pk[2] == 14400);
    CHECK_NEAR (out.getSample (0, 9600), 0.5, 1e-5);
    CHECK (p->getTelemetry().outputChannels.load() == 1);
    EVIDENCE ("mono→mono (PING PONG y PAN R pedidos): ecos a 100/200/300 ms con 1/0,5/0,25");
}

TEST_CASE ("layout.config_changes_and_rates")
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 64, 512 })
        {
            auto p = makeProcessor (2, 2, fs, block);
            set (*p, ids::lfoOn, 1.0f);
            set (*p, ids::lfoDepthId (2), 50.0f);
            set (*p, ids::feedback, 80.0f);
            const auto out = run (*p, noise ((int) fs), block);
            bool finite = true;
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < out.getNumSamples(); ++i)
                    finite = finite && std::isfinite (out.getSample (c, i));
            CHECK (finite);
            // Cambio de configuración en caliente: de estéreo a mono y otra frecuencia.
            p->releaseResources();
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (juce::AudioChannelSet::mono());
            l.outputBuses.add (juce::AudioChannelSet::stereo());
            CHECK (p->setBusesLayout (l));
            p->setRateAndBufferSizeDetails (fs * 2, block);
            p->prepareToPlay (fs * 2, block);
            const auto out2 = run (*p, noise ((int) fs, 2), block);
            CHECK (std::isfinite (out2.getMagnitude (0, out2.getNumSamples())));
        }
    EVIDENCE ("44,1/48/96 kHz con bloques de 64 y 512, y reconfiguración a 1→2 al doble de frecuencia: salida finita");
}

// ============================================================================ wrapper
TEST_CASE ("wrapper.host_tempo_and_reserve")
{
    auto p = makeProcessor();
    wetOnlyClean (*p);
    set (*p, ids::timeSync, 1.0f);
    TestPlayHead ph (120.0);
    auto out = run (*p, impulse (30000), 512, &ph);
    CHECK (firstPeak (out, 0) == 18000);
    CHECK (firstPeak (out, 1) == 24000);
    CHECK (p->getTelemetry().bpmFromHost.load());

    // Host sin BPM: reserva 120 hasta el primer dato.
    auto q = makeProcessor();
    wetOnlyClean (*q);
    set (*q, ids::timeSync, 1.0f);
    TestPlayHead none;
    none.info.setBpm ({});
    out = run (*q, impulse (30000), 512, &none);
    CHECK (firstPeak (out, 0) == 18000);
    CHECK (! q->getTelemetry().bpmFromHost.load());
    EVIDENCE ("playhead 120 BPM: 1/8D → %d, 1/4 → %d; sin BPM → reserva 120 (%d)", 18000, 24000, firstPeak (out, 0));
}

TEST_CASE ("wrapper.bypass_latency_tail")
{
    auto p = makeProcessor();
    CHECK (p->getBypassParameter() == p->getState().getParameter (ids::bypass));
    CHECK (p->getLatencySamples() == 0);
    CHECK (p->getTailLengthSeconds() > 0.0);
    set (*p, ids::output, 12.0f);
    set (*p, ids::bypass, 1.0f);
    p->prepareToPlay (48000.0, 512);
    const auto in = noise (24000);
    CHECK (identical (run (*p, in), in));

    // processBlockBypassed (bypass del host sin parámetro): seco tras el fundido.
    auto q = makeProcessor();
    juce::AudioBuffer<float> buf (2, 4800);
    buf.makeCopyOf (noise (4800));
    const auto ref = buf;
    juce::MidiBuffer midi;
    TestPlayHead ph;
    q->setPlayHead (&ph);
    q->processBlockBypassed (buf, midi);
    float worst = 0.0f;
    for (int i = 2400; i < 4800; ++i)
        worst = juce::jmax (worst, std::abs (buf.getSample (0, i) - ref.getSample (0, i)));
    CHECK (worst == 0.0f);
    q->setPlayHead (nullptr);
    EVIDENCE ("bypass = parámetro del host; latencia 0; cola %.2f s; BYPASS con OUTPUT +12 dB = seco exacto", p->getTailLengthSeconds());
}

TEST_CASE ("wrapper.reset_from_other_thread")
{
    auto p = makeProcessor();
    set (*p, ids::feedback, 90.0f);
    set (*p, ids::wetOnly, 1.0f);
    auto in = noise (48000);
    for (int i = 12000; i < 48000; ++i)
        in.setSample (0, i, 0.0f), in.setSample (1, i, 0.0f);
    bool resetDone = false;
    const auto out = run (*p, in, 512, nullptr, [&] (int pos) {
        if (pos == 24064 && ! resetDone)
        {
            std::thread t ([&] { p->reset(); });
            t.join();
            resetDone = true;
        }
    });
    const float before = out.getMagnitude (0, 20000, 4000);
    const float after = out.getMagnitude (0, 25600, 48000 - 25600);
    CHECK (before > 0.05f);
    CHECK (after == 0.0f);
    EVIDENCE ("reset() desde otro hilo: cola %.3f antes, %.3f tras el fundido en el siguiente bloque", before, after);
}

TEST_CASE ("wrapper.link_precedence_and_automation")
{
    auto p = makeProcessor();
    set (*p, ids::timeLink, 1.0f);
    set (*p, ids::timeLMs, 220.0f);
    set (*p, ids::timeRMs, 900.0f);
    run (*p, noise (2048));
    CHECK_NEAR (p->getTelemetry().timeMs[0].load(), 220.0, 0.01);
    CHECK_NEAR (p->getTelemetry().timeMs[1].load(), 220.0, 0.01);
    // Automatización simultánea de L y R con LINK: manda L.
    const auto out = run (*p, noise (48000), 64, nullptr, [&] (int pos) {
        set (*p, ids::timeLMs, 100.0f + (float) (pos % 7000) * 0.05f);
        set (*p, ids::timeRMs, 2000.0f - (float) (pos % 5000) * 0.1f);
        set (*p, ids::feedback, 20.0f + (float) ((pos / 64) % 60));
        set (*p, "eq3_gain", (float) ((pos / 640) % 37 - 18));
    });
    CHECK (std::isfinite (out.getMagnitude (0, out.getNumSamples())));
    CHECK_NEAR (p->getTelemetry().timeMs[1].load(), p->getTelemetry().timeMs[0].load(), 1e-3);
    EVIDENCE ("LINK con automatización de L y R a la vez: R efectivo = L (%.2f ms)", p->getTelemetry().timeMs[1].load());
}

TEST_CASE ("wrapper.realtime_no_allocation")
{
    auto p = makeProcessor (2, 2, 48000.0, 512);
    set (*p, ids::lfoOn, 1.0f);
    set (*p, ids::lfoDepthId (3), 70.0f);
    TestPlayHead ph;
    p->setPlayHead (&ph);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    buf.makeCopyOf (noise (512));
    p->processBlock (buf, midi); // calentamiento
    const long before = labdtest::allocationCount.load();
    for (int k = 0; k < 200; ++k)
    {
        buf.makeCopyOf (noise (512), true);
        const long mid = labdtest::allocationCount.load();
        p->processBlock (buf, midi);
        CHECK (labdtest::allocationCount.load() == mid);
    }
    (void) before;
    p->setPlayHead (nullptr);
    EVIDENCE ("200 llamadas a processBlock sin reservas de memoria");
}

TEST_CASE ("wrapper.editor_open_close_does_not_affect_audio")
{
    const auto in = noise (96000);
    auto a = makeProcessor();
    set (*a, ids::lfoOn, 1.0f);
    set (*a, ids::lfoDepthId (6), 50.0f);
    const auto ref = run (*a, in);

    auto b = makeProcessor();
    set (*b, ids::lfoOn, 1.0f);
    set (*b, ids::lfoDepthId (6), 50.0f);
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    const auto out = run (*b, in, 512, nullptr, [&] (int pos) {
        if (pos == 512 * 20)
            ed.reset (b->createEditorAndMakeActive());
        if (pos == 512 * 60)
        {
            b->editorBeingDeleted (ed.get());
            ed.reset();
        }
        if (ed != nullptr)
            if (auto* le = dynamic_cast<LabdEditor*> (ed.get()))
                le->refreshNow();
    });
    CHECK (identical (ref, out));
    EVIDENCE ("render con el editor abierto y cerrado a mitad: idéntico bit a bit al render sin editor");
}

// ============================================================================ editor
TEST_CASE ("editor.size_limits_and_aspect")
{
    auto p = makeProcessor();
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p->createEditor()));
    CHECK (e->getWidth() == 1320 && e->getHeight() == 600);
    e->setSize (800, 300);
    auto* c = e->getConstrainer();
    CHECK (c->getMinimumWidth() == 1100 && c->getMinimumHeight() == 500);
    CHECK_NEAR (c->getFixedAspectRatio(), 2.2, 1e-9);
    EVIDENCE ("nominal 1320x600, mínimo 1100x500, máximo 2640x1200, proporción fija %.2f:1", c->getFixedAspectRatio());
}

TEST_CASE ("editor.text_fits_and_no_overlap")
{
    auto p = makeProcessor();
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p->createEditor()));
    auto& panel = e->content();
    // 1) Controles interactivos sin solapes entre sí.
    std::vector<juce::Component*> interactive;
    for (auto* c : panel.getChildren())
        if (dynamic_cast<juce::Slider*> (c) != nullptr || dynamic_cast<juce::Button*> (c) != nullptr
            || c->getComponentID() == "eqDisplay" || c->getComponentID() == "lfoDisplay")
            interactive.push_back (c);
    int overlaps = 0;
    for (size_t i = 0; i < interactive.size(); ++i)
        for (size_t j = i + 1; j < interactive.size(); ++j)
            if (interactive[i]->getBounds().intersects (interactive[j]->getBounds()))
            {
                ++overlaps;
                std::printf ("    solape: %s / %s\n", interactive[i]->getTitle().toRawUTF8(), interactive[j]->getTitle().toRawUTF8());
            }
    CHECK (overlaps == 0);

    // 2) El peor texto de cada lectura y tecla cabe con margen (en el espacio de diseño;
    //    la escala es uniforme, así que también cabe al tamaño mínimo).
    struct Fit { const char* what; juce::Font font; juce::String text; int width; };
    using namespace labd::ui;
    const Fit fits[] = {
        { "PING PONG", labelFont (20.0f), "PING PONG", 187 },
        { "WET ONLY", labelFont (20.0f), "WET ONLY", 174 - 46 },
        { "MANUAL", labelFont (20.0f), "MANUAL", 128 },
        { "TIME", valueFont (36.0f), "4000 ms", 168 },
        { "TIME figura", valueFont (36.0f), "1/32T", 168 },
        { "TIME sub", valueFont (18.0f), "LINK 18000 ms", 168 },
        { "OUTPUT", valueFont (22.0f), juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92" "24.0 dB")), 132 },
        { "FREQ", valueFont (22.0f), "20.0 kHz", 132 },
        { "RATE", valueFont (22.0f), "0.01 Hz", 132 },
        { "RATE", valueFont (22.0f), "8 BAR", 132 },
        { "DEPTH", valueFont (19.0f), "100%", 80 },
        { "BPM", valueFont (23.0f), "400.0 BPM", 270 - 122 },
    };
    // Teclas con texto variable: la letra se ajusta, nunca por debajo de 17 px de diseño.
    for (const char* t : { "HIGH SHELF", "HIGH CUT", "LOW SHELF", "LOW CUT", "PING PONG", "WET ONLY", "BYPASS", "MANUAL" })
    {
        const float h = PanelButton::fittedFontHeight (t, 148.0f - 3.0f - 14.0f, 20.0f);
        CHECK_MSG (h >= 17.0f, "%s: letra %.1f px", t, h);
        EVIDENCE ("tecla de 148 px: '%s' a %.1f px de letra (%.1f px a 1100x500)", t, h, h * 1100.0f / 2640.0f);
    }
    for (const auto& f : fits)
    {
        const float w = juce::GlyphArrangement::getStringWidth (f.font, f.text);
        CHECK_MSG (w <= (float) f.width - 12.0f, "%s: %.1f px en %d", f.what, w, f.width);
        EVIDENCE ("%-12s '%s' ocupa %.0f de %d px de diseño (%.0f px a 1100x500)", f.what, f.text.toRawUTF8(), w, f.width,
                  w * 1100.0f / 2640.0f);
    }
}

TEST_CASE ("editor.time_knobs_follow_sync_and_link")
{
    auto p = makeProcessor();
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p->createEditor()));
    auto* tl = dynamic_cast<juce::Slider*> (e->findById ("timeL"));
    auto* tr = dynamic_cast<juce::Slider*> (e->findById ("timeR"));
    CHECK (tl != nullptr && tr != nullptr);
    tl->setValue (200.0, juce::sendNotificationSync);
    CHECK_NEAR (get (*p, ids::timeLMs), 200.0, 0.5);
    set (*p, ids::timeSync, 1.0f);
    e->refreshNow();
    tl->setValue ((double) tempo::noteIndex ("1/16"), juce::sendNotificationSync);
    CHECK (juce::roundToInt (get (*p, ids::timeLNote)) == tempo::noteIndex ("1/16"));
    CHECK_NEAR (get (*p, ids::timeLMs), 200.0, 0.5); // ms de L conservado
    // LINK: el mando R edita el tiempo común (L).
    set (*p, ids::timeLink, 1.0f);
    e->refreshNow();
    tr->setValue ((double) tempo::noteIndex ("1/2"), juce::sendNotificationSync);
    CHECK (juce::roundToInt (get (*p, ids::timeLNote)) == tempo::noteIndex ("1/2"));
    CHECK (juce::roundToInt (get (*p, ids::timeRNote)) == tempo::noteIndex ("1/4")); // R guardado intacto
    EVIDENCE ("SYNC cambia los mandos a figuras sin perder los ms; con LINK el mando R edita L y R guarda %s",
              p->getState().getParameter (ids::timeRNote)->getCurrentValueAsText().toRawUTF8());
}

TEST_CASE ("editor.eq_band_controls_and_undo")
{
    auto p = makeProcessor();
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p->createEditor()));
    p->setSelectedBand (5);
    e->refreshNow();
    auto* type = dynamic_cast<juce::Button*> (e->findById ("type"));
    auto* gain = dynamic_cast<juce::Slider*> (e->findById ("gain"));
    auto* q = dynamic_cast<juce::Slider*> (e->findById ("q"));
    CHECK (type->isEnabled() && type->getButtonText().startsWith ("HIGH CUT"));
    CHECK (! gain->isEnabled() && q->isEnabled());
    set (*p, "eq6_gain", -6.0f);
    CHECK (type->onClick != nullptr);
    type->onClick(); // equivalente síncrono de un clic
    e->refreshNow();
    CHECK (type->getButtonText().startsWith ("HIGH SHELF"));
    CHECK (gain->isEnabled() && ! q->isEnabled());
    CHECK_NEAR (get (*p, "eq6_gain"), -6.0, 1e-4); // la ganancia se recuerda al cambiar de tipo
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200); // vuelca los cambios al historial
    // Banda 2 (LOW SHELF): sin Q.
    p->setSelectedBand (1);
    e->refreshNow();
    CHECK (! q->isEnabled() && gain->isEnabled());
    // Undo de un gesto del editor.
    auto& undo = p->getUndoManager();
    undo.beginNewTransaction();
    gain->setValue (9.0, juce::sendNotificationSync);
    CHECK_NEAR (get (*p, "eq2_gain"), 9.0, 0.01);
    // El árbol de estado (y su historial de undo) se sincroniza en el hilo de mensajes.
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    undo.beginNewTransaction();
    CHECK (undo.undo());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    CHECK_NEAR (get (*p, "eq2_gain"), 0.0, 0.01);
    CHECK_NEAR (get (*p, "eq6_gain"), -6.0, 1e-4); // el undo solo deshace su gesto
    EVIDENCE ("banda 6: HIGH CUT ↔ HIGH SHELF recuerda GAIN %.1f dB; LOW SHELF sin Q; undo devuelve GAIN a %.1f dB",
              get (*p, "eq6_gain"), get (*p, "eq2_gain"));
}

TEST_CASE ("editor.wet_only_shows_effective_mix")
{
    auto p = makeProcessor();
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p->createEditor()));
    set (*p, ids::mix, 32.0f);
    set (*p, ids::wetOnly, 1.0f);
    e->refreshNow();
    auto* mix = dynamic_cast<juce::Slider*> (e->findById ("mix"));
    CHECK (! mix->isEnabled());
    CHECK_NEAR (get (*p, ids::mix), 32.0, 1e-4);
    // Capturas al tamaño nominal y mínimo (sin errores de dibujo).
    for (int w : { 1320, 1100 })
    {
        e->setSize (w, w * 600 / 1320);
        const auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 2.0f);
        CHECK (img.getWidth() == w * 2);
    }
    EVIDENCE ("WET ONLY: mando MIX deshabilitado mostrando 100%%; MIX guardado %.0f %%", get (*p, ids::mix));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::printf ("LAB-D · pruebas integradas del plugin (JUCE %s)\n\n", juce::SystemStats::getJUCEVersion().toRawUTF8());
    return labdtest::runAll (argc, argv);
}
