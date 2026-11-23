#include "engine/DelayEngine.h"

#include <algorithm>
#include <cmath>

namespace labd {

namespace {
constexpr float kPi = 3.14159265358979f;

// DUCK: detector de pico con retención sobre la señal seca.
constexpr double kDuckEnvAttack = 0.002, kDuckEnvHold = 0.030, kDuckEnvRelease = 0.060;
constexpr double kDuckGainAttack = 0.010, kDuckGainRelease = 0.100;
constexpr float kDuckMaxDb = 36.0f; // atenuación máxima al 100 %

// AUTO: seguidor de la señal seca que atenúa la profundidad del LFO.
constexpr double kAutoEnvAttack = 0.030, kAutoEnvRelease = 0.200;
constexpr float kAutoMaxReduction = 0.85f;

// Zona de detección común: 0 por debajo de -45 dBFS, 1 desde -30 dBFS.
constexpr float kDetectFloorDb = -45.0f, kDetectSpanDb = 15.0f;

constexpr double kBandOctaves = 1.5; // recorrido al 100 % de una banda
constexpr double kPpqJumpTolerance = 0.02; // negras

float levelAmount (float env) noexcept
{
    const float db = gainToDb (env);
    return std::clamp ((db - kDetectFloorDb) / kDetectSpanDb, 0.0f, 1.0f);
}

struct PanGains
{
    float ll = 1.0f, lr = 0.0f, rr = 1.0f, rl = 0.0f;
};

// Desplaza la imagen del wet: en el centro es la identidad; hacia R el canal L
// se traslada a R con ley de potencia constante (y viceversa). Un eco presente
// solo en un canal recorre el estéreo de extremo a extremo sin desaparecer.
PanGains panGains (float p) noexcept
{
    PanGains g;
    const float theta = std::abs (p) * 0.5f * kPi;
    if (p > 0.0f)
    {
        g.ll = std::cos (theta);
        g.lr = std::sin (theta);
    }
    else if (p < 0.0f)
    {
        g.rr = std::cos (theta);
        g.rl = std::sin (theta);
    }
    return g;
}
} // namespace

void DelayEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    fs = sampleRate > 0.0 ? sampleRate : 48000.0;
    channels = std::clamp (numChannels, 1, 2);

    const int maxDelay = (int) std::ceil (tempo::kMaxDelayMs * fs / 1000.0) + 8;
    for (auto& l : lines)
        l.prepare (maxDelay);
    for (auto& r : readers)
        r.prepare (fs, kTimeFadeSeconds);
    for (auto& c : colors)
        c.prepare (fs);
    (void) LoopColor::makeupTables(); // tablas de compensación fuera del hilo de audio
    eq.prepare (fs, 2);

    feedbackS.prepare (fs, 0.020);
    driveS.prepare (fs, 0.030);
    tapeS.prepare (fs, 0.030);
    pingPongS.prepare (fs, 0.030);
    mixS.prepare (fs, 0.020);
    outputS.prepare (fs, 0.020);
    bypassS.prepare (fs, 0.020);
    lfoGateS.prepare (fs, 0.080);
    for (auto& d : depthS)
        d.prepare (fs, 0.050);
    panBaseS.prepare (fs, 0.030);
    ghostS.prepare (fs, 0.010);

    reset();
}

void DelayEngine::hostReset (bool continuousAudio) noexcept
{
    for (auto& l : lines)
        l.forget();
    for (auto& c : colors)
        c.reset();
    if (continuousAudio)
    {
        ghostActive = true;
        ghostS.reset (1.0f);
        ghostS.setTarget (0.0f);
        return;
    }
    ghostActive = false;
    eq.reset();
    lfo.reset (0.0);
    lfoSmoothed = 0.0f;
    duckEnv = autoEnv = 0.0f;
    duckGain = autoScale = 1.0f;
    duckHold = 0;
    wasPlaying = false;
    lfoSynced = false;
}

void DelayEngine::reset()
{
    for (auto& l : lines)
        l.clear();
    for (auto& c : colors)
        c.reset();
    eq.reset();
    lfo.reset (0.0);
    lfoSmoothed = 0.0f;
    duckEnv = autoEnv = 0.0f;
    duckHold = 0;
    duckGain = autoScale = 1.0f;
    wasPlaying = false;
    lfoSynced = false;
    needsSnap = true;
    ghostActive = false;
    bypassCleared = false;
    ghostS.reset (0.0f);
}

void DelayEngine::effectiveTimesMs (const EngineParams& p, double bpm, double& left, double& right) noexcept
{
    using namespace tempo;
    left = p.sync ? noteMs (p.noteL, bpm) : std::clamp (p.timeMsL, kMinTimeMs, kMaxFreeTimeMs);
    right = p.sync ? noteMs (p.noteR, bpm) : std::clamp (p.timeMsR, kMinTimeMs, kMaxFreeTimeMs);
    if (p.link)
        right = left; // LINK: parte del valor de L
    left = std::clamp (left, kMinTimeMs, kMaxDelayMs);
    right = std::clamp (right, kMinTimeMs, kMaxDelayMs);
}

double DelayEngine::lfoFrequency (const EngineParams& p, double bpm, int sigNumerator, int sigDenominator) noexcept
{
    if (p.lfoAuto)
        return (bpm / 60.0) / tempo::barQuarters (sigNumerator, sigDenominator); // AUTO: seno de un compás
    if (p.rateSync)
        return tempo::rateHz (p.rateIndex, bpm, sigNumerator, sigDenominator);
    return std::clamp (p.rateHz, 0.01, 20.0);
}

void DelayEngine::updateTransport (const TransportInfo& t, int numSamples) noexcept
{
    const double b = t.hasBpm ? tempo::sanitiseBpm (t.bpm) : -1.0;
    bpmFromHost = b > 0.0;
    if (bpmFromHost)
        lastBpm = b; // último BPM válido; 120 hasta el primer dato

    if (t.hasTimeSig && t.sigNumerator > 0 && t.sigDenominator > 0)
    {
        sigNum = t.sigNumerator;
        sigDen = t.sigDenominator;
    }

    // Reinicios explícitos del host: al arrancar el transporte o al detectar un
    // salto de posición (locate, ciclo), la fase del LFO sincronizado se alinea
    // con la posición musical. Los cambios normales de BPM no la tocan.
    const bool tempoLocked = params.lfoAuto || params.rateSync;
    if (tempoLocked && t.isPlaying && t.hasPpq && std::isfinite (t.ppq))
    {
        const double cycleQ = params.lfoAuto ? tempo::barQuarters (sigNum, sigDen)
                                             : tempo::rateCycleQuarters (params.rateIndex, sigNum, sigDen);
        const bool jump = ! wasPlaying || ! lfoSynced || std::abs (t.ppq - expectedPpq) > kPpqJumpTolerance;
        if (jump)
            lfo.syncTo (t.ppq / cycleQ);
        lfoSynced = true;
        expectedPpq = t.ppq + (double) numSamples / fs * lastBpm / 60.0;
    }
    else
    {
        lfoSynced = false;
    }
    wasPlaying = t.isPlaying;
}

void DelayEngine::snapSmoothers() noexcept
{
    feedbackS.reset (feedbackS.getTarget());
    driveS.reset (driveS.getTarget());
    tapeS.reset (tapeS.getTarget());
    pingPongS.reset (pingPongS.getTarget());
    mixS.reset (mixS.getTarget());
    outputS.reset (outputS.getTarget());
    bypassS.reset (bypassS.getTarget());
    lfoGateS.reset (lfoGateS.getTarget());
    for (auto& d : depthS)
        d.reset (d.getTarget());
    panBaseS.reset (panBaseS.getTarget());
    panNow = panBaseS.getTarget();
    makeupNow = LoopColor::makeupGain (driveS.getTarget(), tapeS.getTarget());
    needsSnap = false;
}

void DelayEngine::process (float* const* io, int numChannels, int n, const TransportInfo& transport) noexcept
{
    if (n <= 0 || io == nullptr || io[0] == nullptr)
        return;
    const int nch = (numChannels >= 2 && io[1] != nullptr) ? 2 : 1;
    channels = nch;

    updateTransport (transport, n);

    // --- Objetivos del bloque -------------------------------------------------
    double msL = 0.0, msR = 0.0;
    effectiveTimesMs (params, lastBpm, msL, msR);
    if (nch == 1)
        msR = msL; // mono: TIME L y realimentación mono en ambos modos
    const double maxD = (double) lines[0].capacity() - 8.0;
    const double dL = std::clamp (msL * fs / 1000.0, 2.0, maxD);
    const double dR = std::clamp (msR * fs / 1000.0, 2.0, maxD);
    readers[0].setTarget (dL);
    readers[1].setTarget (dR);

    feedbackS.setTarget (std::clamp (params.feedback, 0.0f, 0.98f));
    driveS.setTarget (std::clamp (params.drive, 0.0f, 1.0f));
    tapeS.setTarget (params.tape ? 1.0f : 0.0f);
    pingPongS.setTarget (params.pingPong && nch == 2 ? 1.0f : 0.0f);
    mixS.setTarget (params.wetOnly ? 1.0f : std::clamp (params.mix, 0.0f, 1.0f));
    outputS.setTarget (dbToGain (std::clamp (params.outputDb, -24.0f, 12.0f)));
    bypassS.setTarget (params.bypass ? 1.0f : 0.0f);
    lfoGateS.setTarget (params.lfoOn ? 1.0f : 0.0f);
    for (int i = 0; i < kNumLfoDepths; ++i)
        depthS[(size_t) i].setTarget (std::clamp (params.depth[(size_t) i], 0.0f, 1.0f));
    panBaseS.setTarget (nch == 2 ? std::clamp (params.pan, -1.0f, 1.0f) : 0.0f);
    eq.setParams (params.eq);

    if (needsSnap)
    {
        snapSmoothers();
        readers[0].reset (dL);
        readers[1].reset (dR);
    }

    const double lfoHz = lfoFrequency (params, lastBpm, sigNum, sigDen);
    const LfoShape shape = params.lfoAuto ? LfoShape::Sine : params.shape;
    const double offset = params.lfoAuto ? 0.0 : std::clamp (params.phaseDeg, 0.0, 360.0) / 360.0;
    // Suavizado de aristas: rápido frente al periodo (conserva el carácter):
    // 2 % del periodo, entre 1 y 5 ms.
    const double edgeTau = std::clamp (0.02 / std::max (lfoHz, 1.0e-3), 0.001, 0.005);
    const float duckAmount = std::clamp (params.duck, 0.0f, 1.0f);

    const float dEnvA = (float) onePoleCoefficient (kDuckEnvAttack, fs);
    const float dEnvR = (float) onePoleCoefficient (kDuckEnvRelease, fs);
    const float dGainA = (float) onePoleCoefficient (kDuckGainAttack, fs);
    const float dGainR = (float) onePoleCoefficient (kDuckGainRelease, fs);
    const float aEnvA = (float) onePoleCoefficient (kAutoEnvAttack, fs);
    const float aEnvR = (float) onePoleCoefficient (kAutoEnvRelease, fs);
    const int duckHoldSamples = (int) (kDuckEnvHold * fs);

    float* outL = io[0];
    float* outR = nch == 2 ? io[1] : nullptr;

    std::array<float, kNumLfoDepths> d {};

    for (int pos = 0; pos < n; pos += kSubBlock)
    {
        const int len = std::min (kSubBlock, n - pos);

        // --- LFO (por sub-bloque) ---------------------------------------------
        lfo.advance ((double) len * lfoHz / fs);
        const float raw = lfo.value (shape, offset);
        lfoSmoothed += (float) onePoleCoefficient (edgeTau, fs / len) * (raw - lfoSmoothed);
        const float gate = lfoGateS.skip (len);
        const float scale = params.lfoAuto ? autoScale : 1.0f;
        for (int i = 0; i < kNumLfoDepths; ++i)
            d[(size_t) i] = depthS[(size_t) i].skip (len) * gate * scale;

        double mult[kNumEqBands];
        for (int b = 0; b < kNumEqBands; ++b)
            mult[b] = d[(size_t) b] == 0.0f ? 1.0 : std::exp2 (kBandOctaves * d[(size_t) b] * lfoSmoothed);
        eq.updateBlock (mult, len);

        const float panPrev = panNow;
        const float panBase = panBaseS.skip (len);
        panNow = nch == 2 ? std::clamp (panBase + d[kPanDepthIndex] * lfoSmoothed, -1.0f, 1.0f) : 0.0f;
        const PanGains g0 = panGains (panPrev), g1 = panGains (panNow);

        // --- Bucle de retardo (por muestra) -----------------------------------
        float dryL[kSubBlock], dryR[kSubBlock], wetL[kSubBlock], wetR[kSubBlock], bypass[kSubBlock];
        const float lpA = colors[0].lowPassCoefficient (driveS.getCurrent());
        const float makeupPrev = makeupNow;

        for (int s = 0; s < len; ++s)
        {
            const float xL = outL[pos + s];
            const float xR = outR != nullptr ? outR[pos + s] : xL;
            dryL[s] = xL;
            dryR[s] = xR;

            // Detectores: una muestra no finita no debe dejarlos en NaN/Inf para siempre.
            float level = std::max (std::abs (xL), std::abs (xR));
            if (! (level < 1.0e6f))
                level = 0.0f;
            if (level > duckEnv)
            {
                duckEnv += dEnvA * (level - duckEnv);
                duckHold = duckHoldSamples;
            }
            else if (duckHold > 0)
                --duckHold;
            else
                duckEnv += dEnvR * (level - duckEnv);
            autoEnv += (level > autoEnv ? aEnvA : aEnvR) * (level - autoEnv);

            const float fb = feedbackS.next();
            const float drv = driveS.next();
            const float tp = tapeS.next();
            const float pp = pingPongS.next();
            const float byp = bypassS.next();
            bypass[s] = byp;
            // Con BYPASS la entrada deja de grabarse en el bucle (rampa con el bypass).
            const float send = 1.0f - byp;
            const float sL = send == 1.0f ? xL : xL * send;
            const float sR = send == 1.0f ? xR : xR * send;
            // Cola anterior a un reinicio del host: solo a la salida, nunca al bucle.
            const float ghostGain = ghostActive ? ghostS.next() : 0.0f;

            float gL = 0.0f, gR = 0.0f;
            const float eL = readers[0].process (lines[0], ghostActive ? &gL : nullptr);
            float inL = 0.0f;
            if (nch == 1)
            {
                inL = sL + fb * eL;
                wetL[s] = ghostActive ? eL + ghostGain * gL : eL;
                wetR[s] = 0.0f;
            }
            else
            {
                const float eR = readers[1].process (lines[1], ghostActive ? &gR : nullptr);
                inL = sL + fb * eL;
                float inR = sR + fb * eR;
                if (pp > 0.0f)
                {
                    // PING PONG: la entrada (suma mono) entra por L y la
                    // realimentación se cruza L <-> R.
                    const float ppL = 0.5f * (sL + sR) + fb * eR;
                    const float ppR = fb * eL;
                    inL += pp * (ppL - inL);
                    inR += pp * (ppR - inR);
                }
                float wR = colors[1].process (inR, drv, tp, lpA);
                if (! (std::abs (wR) < 1.0e6f))
                    wR = 0.0f; // protección del bucle ante NaN/Inf
                lines[1].push (wR);
                wetL[s] = ghostActive ? eL + ghostGain * gL : eL;
                wetR[s] = ghostActive ? eR + ghostGain * gR : eR;
            }
            float wL = colors[0].process (inL, drv, tp, lpA);
            if (! (std::abs (wL) < 1.0e6f))
                wL = 0.0f;
            lines[0].push (wL);
        }

        // --- Compensación de DRIVE/TAPE (fuera del bucle) ----------------------
        makeupNow = LoopColor::makeupGain (driveS.getCurrent(), tapeS.getCurrent());
        if (makeupPrev != 1.0f || makeupNow != 1.0f)
        {
            const float invL = 1.0f / (float) len;
            for (int s = 0; s < len; ++s)
            {
                const float m = makeupPrev + (makeupNow - makeupPrev) * (float) (s + 1) * invL;
                wetL[s] *= m;
                wetR[s] *= m;
            }
        }

        // --- ECHO EQ (solo wet, fuera del bucle) ------------------------------
        eq.process (wetL, nch == 2 ? wetR : nullptr, len);

        // --- AUTO y DUCK ------------------------------------------------------
        autoScale = 1.0f - kAutoMaxReduction * levelAmount (autoEnv);
        const float duckTarget = duckAmount > 0.0f ? dbToGain (-kDuckMaxDb * duckAmount * levelAmount (duckEnv)) : 1.0f;

        // --- PAN → DUCK → MIX → OUTPUT → BYPASS -------------------------------
        const float invLen = 1.0f / (float) len;
        for (int s = 0; s < len; ++s)
        {
            float wl = wetL[s];
            float wr = wetR[s];
            if (nch == 2)
            {
                const float t = (float) (s + 1) * invLen;
                const float ll = g0.ll + (g1.ll - g0.ll) * t, lr = g0.lr + (g1.lr - g0.lr) * t;
                const float rr = g0.rr + (g1.rr - g0.rr) * t, rl = g0.rl + (g1.rl - g0.rl) * t;
                const float a = wl, b = wr;
                wl = a * ll + b * rl;
                wr = b * rr + a * lr;
            }

            duckGain += (duckTarget < duckGain ? dGainA : dGainR) * (duckTarget - duckGain);
            if (std::abs (duckTarget - duckGain) < 1.0e-6f)
                duckGain = duckTarget;
            wl *= duckGain;
            wr *= duckGain;

            const float m = mixS.next();
            const float og = outputS.next();
            const float byp = bypass[s];

            float oL = (dryL[s] * (1.0f - m) + wl * m) * og;
            if (byp >= 1.0f)
                oL = dryL[s];
            else if (byp > 0.0f)
                oL += byp * (dryL[s] - oL);
            outL[pos + s] = oL;

            if (outR != nullptr)
            {
                float oR = (dryR[s] * (1.0f - m) + wr * m) * og;
                if (byp >= 1.0f)
                    oR = dryR[s];
                else if (byp > 0.0f)
                    oR += byp * (dryR[s] - oR);
                outR[pos + s] = oR;
            }
        }

        if (ghostActive && ! ghostS.isSmoothing() && ghostS.getCurrent() == 0.0f)
            ghostActive = false;

        // BYPASS completo: la salida ya es el seco exacto, así que la cola se vacía
        // (en silencio). Al salir del bypass el delay arranca limpio, igual en AU
        // (donde JUCE emite además un Reset al activar el bypass) y en VST3.
        if (bypassS.getCurrent() >= 1.0f && ! bypassS.isSmoothing())
        {
            if (! bypassCleared)
            {
                for (auto& l : lines)
                    l.forget();
                for (auto& c : colors)
                    c.reset();
                ghostActive = false;
                bypassCleared = true;
            }
        }
        else
            bypassCleared = false;
    }

    // --- Telemetría ------------------------------------------------------------
    telem.effectiveTimeMs[0] = msL;
    telem.effectiveTimeMs[1] = msR;
    telem.bpm = lastBpm;
    telem.bpmFromHost = bpmFromHost;
    telem.sigNumerator = sigNum;
    telem.sigDenominator = sigDen;
    telem.lfoHz = lfoHz;
    telem.lfoPhase = lfo.phase (offset);
    telem.lfoGate = lfoGateS.getCurrent();
    telem.lfoValue = lfoSmoothed * telem.lfoGate;
    telem.autoScale = params.lfoAuto ? autoScale : 1.0f;
    telem.effectiveDepth = d;
    telem.effectivePan = panNow;
    for (int b = 0; b < kNumEqBands; ++b)
    {
        telem.eqBaseFreq[(size_t) b] = eq.baseFrequency (b);
        telem.eqEffectiveFreq[(size_t) b] = eq.effectiveFrequency (b);
        telem.eqBandAmount[(size_t) b] = eq.bandAmount (b);
    }
    telem.eqGlobalAmount = eq.globalAmount();
    telem.duckGainDb = gainToDb (duckGain);
    telem.effectiveMix = mixS.getCurrent();
    telem.numChannels = nch;
}

} // namespace labd
