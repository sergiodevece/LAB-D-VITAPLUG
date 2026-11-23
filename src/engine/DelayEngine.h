// VitaPlug LAB-D · motor de audio (sin JUCE)
//
// Ruta: líneas de retardo con feedback y color → ECHO EQ → PAN → DUCK → MIX
// con la ruta seca → OUTPUT. BYPASS entrega la señal seca a ganancia unitaria,
// por fuera de OUTPUT. Latencia cero.
//
// El motor no depende de JUCE ni de un DAW: recibe valores planos de
// parámetros y la información de transporte por bloque.
#pragma once

#include "engine/DelayLine.h"
#include "engine/EchoEq.h"
#include "engine/Lfo.h"
#include "engine/LoopColor.h"
#include "engine/Smoothers.h"
#include "engine/TempoMath.h"

#include <array>
#include <cstdint>

namespace labd {

constexpr int kNumLfoDepths = 7; // bandas 1-6 y PAN
constexpr int kPanDepthIndex = 6;

struct EngineParams
{
    // Tiempo
    bool sync = false;
    bool link = false;
    double timeMsL = 350.0, timeMsR = 500.0;
    int noteL = 9;  // 1/8D
    int noteR = 10; // 1/4

    // Bucle
    float feedback = 0.30f; // 0..0,98
    bool pingPong = false;
    bool tape = false;
    float drive = 0.0f; // 0..1
    float duck = 0.0f;  // 0..1

    // Salida
    float mix = 0.25f; // 0..1
    bool wetOnly = false;
    float outputDb = 0.0f;
    bool bypass = false;

    // Wet
    EqParams eq {};
    float pan = 0.0f; // -1 (L) .. +1 (R)

    // LFO
    bool lfoOn = false;
    bool lfoAuto = false;
    LfoShape shape = LfoShape::Sine;
    bool rateSync = false;
    double rateHz = 1.0;
    int rateIndex = 8; // 1/2
    double phaseDeg = 0.0;
    std::array<float, kNumLfoDepths> depth {}; // 0..1
};

struct TransportInfo
{
    bool hasBpm = false;
    double bpm = 0.0;
    bool hasPpq = false;
    double ppq = 0.0;
    bool isPlaying = false;
    bool hasTimeSig = false;
    int sigNumerator = 4, sigDenominator = 4;
};

struct EngineTelemetry
{
    double effectiveTimeMs[2] {};
    double bpm = tempo::kDefaultBpm;
    bool bpmFromHost = false;
    int sigNumerator = 4, sigDenominator = 4;
    double lfoHz = 0.0;
    double lfoPhase = 0.0;       // 0..1 incluida la fase PHASE
    float lfoValue = 0.0f;       // onda suavizada · encendido
    float lfoGate = 0.0f;        // 0..1 (rampa ON/OFF)
    float autoScale = 1.0f;      // escala de profundidad en AUTO
    std::array<float, kNumLfoDepths> effectiveDepth {};
    float effectivePan = 0.0f;
    std::array<double, kNumEqBands> eqBaseFreq {}, eqEffectiveFreq {};
    std::array<float, kNumEqBands> eqBandAmount {};
    float eqGlobalAmount = 1.0f;
    float duckGainDb = 0.0f;
    float effectiveMix = 0.25f;
    int numChannels = 2;
};

class DelayEngine {
public:
    static constexpr int kSubBlock = 16;
    static constexpr double kTimeFadeSeconds = 0.050;

    // Reserva todo lo necesario para el rango musical declarado (hasta 18 s).
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    // Reinicio explícito: vacía líneas y estados (colas incluidas).
    void reset();

    void setParams (const EngineParams& p) noexcept { params = p; }
    const EngineParams& getParams() const noexcept { return params; }

    // Procesa en sitio. channels[c] es entrada seca y salida. numChannels 1 o 2.
    void process (float* const* channels, int numChannels, int numSamples, const TransportInfo& transport) noexcept;

    const EngineTelemetry& telemetry() const noexcept { return telem; }
    double getSampleRate() const noexcept { return fs; }
    int getNumChannels() const noexcept { return channels; }
    size_t delayCapacitySamples() const noexcept { return lines[0].capacity(); }

    // Tiempo efectivo (ms) de cada canal para unos parámetros y un BPM.
    static void effectiveTimesMs (const EngineParams& p, double bpm, double& left, double& right) noexcept;
    // Frecuencia efectiva del LFO.
    static double lfoFrequency (const EngineParams& p, double bpm, int sigNum, int sigDen) noexcept;

private:
    void updateTransport (const TransportInfo& t, int numSamples) noexcept;
    void snapSmoothers() noexcept;

    double fs = 48000.0;
    int channels = 2;
    EngineParams params {};
    EngineTelemetry telem {};
    bool needsSnap = true;

    std::array<DelayLine, 2> lines;
    std::array<CrossfadeReader, 2> readers;
    std::array<LoopColor, 2> colors;
    EchoEq eq;
    Lfo lfo;

    LinearSmoother feedbackS, driveS, tapeS, pingPongS, mixS, outputS, bypassS, lfoGateS;
    std::array<LinearSmoother, kNumLfoDepths> depthS;
    LinearSmoother panBaseS;

    // Tempo
    double lastBpm = tempo::kDefaultBpm;
    bool bpmFromHost = false;
    int sigNum = 4, sigDen = 4;
    bool wasPlaying = false;
    double expectedPpq = 0.0;
    bool lfoSynced = false;

    // LFO
    float lfoSmoothed = 0.0f;
    float panNow = 0.0f;

    // Detectores de la señal seca
    float duckEnv = 0.0f, duckGain = 1.0f;
    int duckHold = 0;
    float autoEnv = 0.0f, autoScale = 1.0f;
};

} // namespace labd
