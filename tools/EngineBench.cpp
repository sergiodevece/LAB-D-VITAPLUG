// VitaPlug LAB-D · banco de CPU del motor (sin JUCE). Ejecutar en Release.
//   labd_engine_bench [segundos]
// Mide el coste del motor con EQ (6 bandas) y LFO (7 destinos) activos, con 1 y
// 8 instancias, a 48 kHz y bloques de 64 y 512. Incluye CLEAN y TAPE con DRIVE.
#include "engine/DelayEngine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

using namespace labd;

static EngineParams fullParams (bool tape, float drive)
{
    EngineParams p;
    p.feedback = 0.6f;
    p.mix = 0.4f;
    p.sync = true;
    p.noteL = tempo::noteIndex ("1/8D");
    p.noteR = tempo::noteIndex ("1/4");
    p.pingPong = true;
    p.tape = tape;
    p.drive = drive;
    p.duck = 0.3f;
    p.eq.eqOn = true;
    const double f[6] = { 85, 225, 640, 1500, 3750, 8100 };
    for (int b = 0; b < 6; ++b)
    {
        p.eq.band[(size_t) b].on = true;
        p.eq.band[(size_t) b].freq = f[b];
        p.eq.band[(size_t) b].gainDb = 3.0;
        p.eq.band[(size_t) b].q = 1.1;
    }
    p.lfoOn = true;
    p.rateSync = true;
    p.rateIndex = tempo::rateIndex ("1/16T");
    for (auto& d : p.depth)
        d = 0.4f;
    return p;
}

int main (int argc, char** argv)
{
    const double seconds = argc > 1 ? std::atof (argv[1]) : 20.0;
    const double fs = 48000.0;
#ifdef NDEBUG
    const char* build = "Release";
#else
    const char* build = "Debug (no representativo)";
#endif
    std::printf ("LAB-D motor · %s · %.0f s de audio por caso · 48 kHz estéreo\n", build, seconds);
    std::printf ("%-26s %9s %10s %12s %12s\n", "caso", "bloque", "instancias", "us/bloque", "%% tiempo real");

    struct Mode { const char* name; bool tape; float drive; bool eqLfo; };
    const Mode modes[] = {
        { "EQ+LFO, CLEAN, DRIVE 0", false, 0.0f, true },
        { "EQ+LFO, CLEAN, DRIVE 60", false, 0.6f, true },
        { "EQ+LFO, TAPE, DRIVE 60", true, 0.6f, true },
        { "sin EQ/LFO, CLEAN", false, 0.0f, false },
    };
    for (const auto& m : modes)
        for (int block : { 64, 512 })
            for (int instances : { 1, 8 })
            {
                std::vector<std::unique_ptr<DelayEngine>> engines;
                auto params = fullParams (m.tape, m.drive);
                if (! m.eqLfo)
                {
                    params.eq.eqOn = false;
                    params.lfoOn = false;
                }
                for (int i = 0; i < instances; ++i)
                {
                    engines.push_back (std::make_unique<DelayEngine>());
                    engines.back()->prepare (fs, block, 2);
                    engines.back()->setParams (params);
                }
                std::vector<float> l ((size_t) block), r ((size_t) block);
                TransportInfo t;
                t.hasBpm = true;
                t.bpm = 120.0;
                t.isPlaying = true;
                t.hasPpq = true;
                const long blocks = (long) (seconds * fs / block);
                unsigned seed = 1;
                double ppq = 0.0;
                const auto t0 = std::chrono::steady_clock::now();
                for (long k = 0; k < blocks; ++k)
                {
                    t.ppq = ppq;
                    ppq += block / fs * 2.0;
                    for (auto& e : engines)
                    {
                        for (int i = 0; i < block; ++i)
                        {
                            seed = seed * 1664525u + 1013904223u;
                            l[(size_t) i] = r[(size_t) i] = ((float) (seed >> 9) / 8388608.0f - 1.0f) * 0.3f;
                        }
                        float* ch[2] = { l.data(), r.data() };
                        e->process (ch, 2, block, t);
                    }
                }
                const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                const double usPerBlock = elapsed / (double) blocks * 1e6;
                const double rt = elapsed / seconds * 100.0;
                std::printf ("%-26s %9d %10d %12.2f %11.3f%%\n", m.name, block, instances, usPerBlock, rt);
            }
    return 0;
}
