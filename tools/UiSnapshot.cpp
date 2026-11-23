// VitaPlug LAB-D · captura del editor real renderizado fuera de pantalla.
//   labd_ui_snapshot <carpeta_salida>
// Genera: editor_init_1320.png, editor_maqueta_1320.png (ajuste de la maqueta,
// ver docs/FICHA_MAQUETA.md), editor_maqueta_min_1100.png y editor_mono.png,
// todas a escala 2x (Retina).
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

#include <juce_audio_utils/juce_audio_utils.h>

using namespace labd;

namespace {
struct SnapPlayHead final : juce::AudioPlayHead
{
    PositionInfo info;
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};

void set (LabdProcessor& p, const juce::String& id, float v)
{
    auto* q = p.getState().getParameter (id);
    q->setValueNotifyingHost (q->convertTo0to1 (v));
}

// Ajuste de demostración de la maqueta (LAB-D_Frontal.png / LAB-D_Interfaz.html).
void applyMockupSetting (LabdProcessor& p)
{
    set (p, ids::timeSync, 1.0f);
    set (p, ids::timeLNote, (float) tempo::noteIndex ("1/8D"));
    set (p, ids::timeRNote, (float) tempo::noteIndex ("1/4"));
    set (p, ids::feedback, 42.0f);
    set (p, ids::mix, 32.0f);
    set (p, ids::output, 0.0f);
    const double f[6] = { 85, 225, 640, 1500, 3750, 8100 };
    const double g[6] = { 0, -1.5, -3.5, 3.0, -1.8, 0 };
    const double q[6] = { 0.71, 0.71, 1.1, 0.95, 1.2, 0.71 };
    for (int b = 0; b < kNumEqBands; ++b)
    {
        set (p, ids::eqOnId (b), 1.0f);
        set (p, ids::eqFreqId (b), (float) f[b]);
        if (ids::bandHasGain (b))
            set (p, ids::eqGainId (b), (float) g[b]);
        if (ids::bandHasQ (b))
            set (p, ids::eqQId (b), (float) q[b]);
    }
    set (p, ids::lfoOn, 1.0f);
    set (p, ids::lfoRateSync, 1.0f);
    set (p, ids::lfoRateNote, (float) tempo::rateIndex ("1/2"));
    set (p, ids::lfoDepthId (2), 44.0f);
    set (p, ids::lfoDepthId (5), 26.0f);
    set (p, ids::lfoDepthId (6), 38.0f);
    p.setSelectedBand (2);
}

void runAudio (LabdProcessor& p, double seconds, SnapPlayHead& ph)
{
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    juce::Random rnd (3);
    for (int k = 0; k < (int) (seconds * 48000.0 / 512); ++k)
    {
        for (int c = 0; c < buf.getNumChannels(); ++c)
            for (int i = 0; i < 512; ++i)
                buf.setSample (c, i, (rnd.nextFloat() * 2.0f - 1.0f) * 0.01f);
        p.processBlock (buf, midi);
        ph.info.setPpqPosition (*ph.info.getPpqPosition() + 512.0 / 48000.0 * 2.0);
    }
}

bool snap (LabdProcessor& p, int width, const juce::File& file)
{
    std::unique_ptr<LabdEditor> e (dynamic_cast<LabdEditor*> (p.createEditor()));
    e->setSize (width, width * 1200 / 2640);
    e->refreshNow();
    const auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    const bool ok = out.openedOk() && png.writeImageToStream (img, out);
    std::printf ("%s %s (%dx%d)\n", ok ? "OK " : "ERR", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    p.editorBeingDeleted (e.get());
    return ok;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    dir.createDirectory();
    SnapPlayHead ph;
    ph.info.setBpm (120.0);
    ph.info.setIsPlaying (true);
    ph.info.setPpqPosition (0.0);
    ph.info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });

    bool ok = true;
    {
        LabdProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setPlayHead (&ph);
        runAudio (p, 0.5, ph);
        ok &= snap (p, 1320, dir.getChildFile ("editor_init_1320.png"));
        p.setPlayHead (nullptr);
    }
    {
        LabdProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        applyMockupSetting (p);
        p.setPlayHead (&ph);
        ph.info.setPpqPosition (0.0);
        runAudio (p, 0.13, ph); // fase del LFO cerca de la maqueta
        ok &= snap (p, 1320, dir.getChildFile ("editor_maqueta_1320.png"));
        ok &= snap (p, 1100, dir.getChildFile ("editor_maqueta_min_1100.png"));
        p.setPlayHead (nullptr);
    }
    {
        LabdProcessor p;
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (juce::AudioChannelSet::mono());
        l.outputBuses.add (juce::AudioChannelSet::mono());
        p.setBusesLayout (l);
        p.setRateAndBufferSizeDetails (48000.0, 512);
        p.prepareToPlay (48000.0, 512);
        p.setPlayHead (&ph);
        runAudio (p, 0.2, ph);
        ok &= snap (p, 1320, dir.getChildFile ("editor_mono_1320.png"));
        p.setPlayHead (nullptr);
    }
    return ok ? 0 : 1;
}
