// VitaPlug LAB-D · banco de CPU del procesador completo (wrapper JUCE + motor).
//   labd_plugin_bench [segundos]
// 1 y 8 instancias, 48 kHz, bloques de 64 y 512, EQ (6 bandas) y LFO (7 destinos) activos.
#include "plugin/PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <chrono>

using namespace labd;

namespace {
struct BenchPlayHead final : juce::AudioPlayHead
{
    PositionInfo info;
    juce::Optional<PositionInfo> getPosition() const override { return info; }
};

void set (LabdProcessor& p, const juce::String& id, float v)
{
    auto* q = p.getState().getParameter (id);
    q->setValueNotifyingHost (q->convertTo0to1 (v));
}

void configure (LabdProcessor& p)
{
    set (p, ids::timeSync, 1.0f);
    set (p, ids::stereoMode, 1.0f);
    set (p, ids::color, 1.0f);
    set (p, ids::drive, 40.0f);
    set (p, ids::feedback, 60.0f);
    set (p, ids::duck, 30.0f);
    for (int b = 0; b < kNumEqBands; ++b)
    {
        set (p, ids::eqOnId (b), 1.0f);
        if (ids::bandHasGain (b))
            set (p, ids::eqGainId (b), 3.0f);
    }
    set (p, ids::lfoOn, 1.0f);
    set (p, ids::lfoRateSync, 1.0f);
    set (p, ids::lfoRateNote, (float) tempo::rateIndex ("1/16T"));
    for (int i = 0; i < kNumLfoDepths; ++i)
        set (p, ids::lfoDepthId (i), 40.0f);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double seconds = argc > 1 ? std::atof (argv[1]) : 20.0;
    const double fs = 48000.0;
#ifdef NDEBUG
    const char* build = "Release";
#else
    const char* build = "Debug (no representativo)";
#endif
    std::printf ("LAB-D procesador · %s · %s · %.0f s por caso · 48 kHz estéreo · TAPE+DRIVE, PING PONG, DUCK, EQ 6 bandas, LFO 7 destinos\n",
                 build, juce::SystemStats::getCpuModel().toRawUTF8(), seconds);
    std::printf ("%8s %11s %13s %15s\n", "bloque", "instancias", "us/bloque", "% tiempo real");
    for (int block : { 64, 512 })
        for (int instances : { 1, 8 })
        {
            std::vector<std::unique_ptr<LabdProcessor>> procs;
            BenchPlayHead ph;
            ph.info.setBpm (120.0);
            ph.info.setIsPlaying (true);
            ph.info.setPpqPosition (0.0);
            for (int i = 0; i < instances; ++i)
            {
                auto p = std::make_unique<LabdProcessor>();
                p->setRateAndBufferSizeDetails (fs, block);
                p->prepareToPlay (fs, block);
                configure (*p);
                p->setPlayHead (&ph);
                procs.push_back (std::move (p));
            }
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            juce::Random rnd (1);
            const long blocks = (long) (seconds * fs / block);
            double ppq = 0.0;
            const auto t0 = std::chrono::steady_clock::now();
            for (long k = 0; k < blocks; ++k)
            {
                ph.info.setPpqPosition (ppq);
                ppq += block / fs * 2.0;
                for (auto& p : procs)
                {
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < block; ++i)
                            buf.setSample (c, i, (rnd.nextFloat() * 2.0f - 1.0f) * 0.3f);
                    p->processBlock (buf, midi);
                }
            }
            const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("%8d %11d %13.2f %14.3f%%\n", block, instances, elapsed / (double) blocks * 1e6, elapsed / seconds * 100.0);
            for (auto& p : procs)
                p->setPlayHead (nullptr);
        }
    return 0;
}
