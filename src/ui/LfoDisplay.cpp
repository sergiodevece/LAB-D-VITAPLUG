#include "ui/LfoDisplay.h"

namespace labd::ui {

namespace {
// Geometría local (frontal: pantalla 1813,279 - 2530,511).
constexpr float kWaveLeft = 33.0f, kWaveRight = 683.0f, kWaveMid = 115.0f, kWaveAmp = 52.0f;
constexpr float kPanY = 209.0f, kPanLeft = 122.0f, kPanRight = 595.0f;
} // namespace

LfoDisplay::LfoDisplay (LabdProcessor& p) : proc (p)
{
    refresh();
}

void LfoDisplay::refresh()
{
    auto& s = proc.getState();
    auto v = [&] (const juce::String& id) { return s.getRawParameterValue (id)->load(); };
    const auto& tl = proc.getTelemetry();
    on = v (ids::lfoOn) >= 0.5f;
    autoMode = juce::roundToInt (v (ids::lfoMode)) == 1;
    shape = autoMode ? LfoShape::Sine : (LfoShape) juce::jlimit (0, 4, juce::roundToInt (v (ids::lfoShape)));
    phaseOffset = autoMode ? 0.0f : v (ids::lfoPhase) / 360.0f;
    phase = tl.lfoPhase.load();
    value = tl.lfoValue.load();
    gate = tl.lfoGate.load();
    autoScale = tl.autoScale.load();
    effPan = tl.effectivePan.load();
    basePan = v (ids::pan) * 0.01f;
    mono = tl.outputChannels.load() < 2;

    if (autoMode)
        rateLabel = "1 BAR";
    else if (v (ids::lfoRateSync) >= 0.5f)
        rateLabel = tempo::kRates[juce::jlimit (0, tempo::kNumRates - 1, juce::roundToInt (v (ids::lfoRateNote)))].label;
    else
        rateLabel = format::hz (v (ids::lfoRate));

    // Repinta mientras el LFO se mueve o cuando cambia algo visible.
    const auto key = rateLabel + "|" + juce::String ((int) shape) + (on ? "1" : "0") + (autoMode ? "1" : "0") + (mono ? "1" : "0")
                     + juce::String (basePan, 3) + juce::String (effPan, 3) + juce::String (phaseOffset, 3);
    if (on || gate > 0.0f || key != lastKey)
    {
        lastKey = key;
        repaint();
    }
}

float LfoDisplay::panX (float pan) const noexcept
{
    return kPanLeft + (juce::jlimit (-1.0f, 1.0f, pan) * 0.5f + 0.5f) * (kPanRight - kPanLeft);
}

void LfoDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 15.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff121d14), 0, 0, juce::Colour (0xff0a120c), 0, r.getHeight(), false));
    g.fillRoundedRectangle (r.reduced (2.0f), 14.0f);
    g.setColour (juce::Colour (0xff7d8c6c));
    g.drawRoundedRectangle (r.reduced (2.0f), 14.0f, 2.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (4.5f), 12.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.018f));
    for (float y = 8.0f; y < r.getHeight() - 8.0f; y += 4.0f)
        g.drawHorizontalLine ((int) y, 8.0f, r.getWidth() - 8.0f);

    const bool active = on || gate > 0.0f;
    // Retícula
    g.setColour (juce::Colour (0x7091a972));
    for (float y : { kWaveMid - kWaveAmp, kWaveMid, kWaveMid + kWaveAmp })
        g.drawHorizontalLine (juce::roundToInt (y), kWaveLeft, kWaveRight);
    g.setColour (juce::Colour (0x3099b673));
    for (int i = 0; i <= 8; ++i)
        g.drawVerticalLine (juce::roundToInt (kWaveLeft + (kWaveRight - kWaveLeft) * (float) i / 8.0f), 42.0f, 188.0f);

    // Forma de onda (un ciclo). En AUTO, la amplitud refleja la profundidad efectiva.
    auto waveAt = [this] (float p) { return lfoWave (shape, (double) p, kLfoDefaultSeed); };
    const float amp = kWaveAmp * (autoMode ? juce::jlimit (0.15f, 1.0f, autoScale) : 1.0f);
    if (autoMode)
    {
        juce::Path ghost;
        for (int i = 0; i <= 240; ++i)
        {
            const float p = (float) i / 240.0f;
            const float y = kWaveMid - waveAt (p) * kWaveAmp;
            i == 0 ? ghost.startNewSubPath (kWaveLeft, y) : ghost.lineTo (kWaveLeft + p * (kWaveRight - kWaveLeft), y);
        }
        g.setColour (colours::amberDim.withAlpha (0.35f));
        g.strokePath (ghost, juce::PathStrokeType (1.2f));
    }
    juce::Path wave;
    for (int i = 0; i <= 240; ++i)
    {
        const float p = (float) i / 240.0f;
        const float y = kWaveMid - waveAt (p) * amp;
        i == 0 ? wave.startNewSubPath (kWaveLeft, y) : wave.lineTo (kWaveLeft + p * (kWaveRight - kWaveLeft), y);
    }
    g.setColour (active ? colours::amber : colours::amberDim);
    g.strokePath (wave, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (active)
    {
        const float x = kWaveLeft + phase * (kWaveRight - kWaveLeft);
        const float y = kWaveMid - waveAt (phase) * amp;
        g.setColour (colours::amber.withAlpha (0.45f));
        g.drawVerticalLine (juce::roundToInt (x), 42.0f, 190.0f);
        g.setColour (juce::Colour (0xfff8d294));
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
    }

    g.setFont (valueFont (22.0f));
    g.setColour (colours::amber);
    g.drawText (rateLabel, juce::Rectangle<float> (kWaveLeft, 10.0f, 260.0f, 28.0f), juce::Justification::centredLeft);
    g.setFont (valueFont (18.0f));
    g.setColour (colours::cream.withAlpha (0.85f));
    const juce::String status = ! on ? "OFF" : (autoMode ? "AUTO " + juce::String (juce::roundToInt (autoScale * 100.0f)) + "%" : "CYCLE");
    g.drawText (status, juce::Rectangle<float> (kWaveRight - 260.0f, 10.0f, 260.0f, 28.0f), juce::Justification::centredRight);

    // Panorama efectivo del wet
    g.setFont (valueFont (17.0f));
    g.drawText ("L", juce::Rectangle<float> (kWaveLeft, kPanY - 12.0f, 30.0f, 24.0f), juce::Justification::centredLeft);
    g.drawText ("R", juce::Rectangle<float> (kWaveRight - 30.0f, kPanY - 12.0f, 30.0f, 24.0f), juce::Justification::centredRight);
    g.setColour (juce::Colour (0x8876876b));
    g.drawLine (kPanLeft, kPanY, kPanRight, kPanY, 2.0f);
    g.setColour (juce::Colour (0xffacb886));
    const float mid = panX (0.0f);
    g.drawLine (mid, kPanY - 8.0f, mid, kPanY + 8.0f, 2.0f);
    if (mono)
    {
        g.setColour (colours::creamDim);
        g.drawText ("MONO", juce::Rectangle<float> (mid - 60.0f, kPanY - 30.0f, 120.0f, 20.0f), juce::Justification::centred);
        return;
    }
    // PAN base (marca) y PAN efectivo (punto ámbar).
    if (std::abs (basePan) > 0.005f)
    {
        g.setColour (colours::cream.withAlpha (0.75f));
        const float bx = panX (basePan);
        g.drawLine (bx, kPanY - 7.0f, bx, kPanY + 7.0f, 2.5f);
    }
    g.setColour (active || std::abs (effPan) > 0.0f ? juce::Colour (0xfff0c282) : juce::Colour (0xff8e8d74));
    const float ex = panX (effPan);
    g.fillRoundedRectangle (ex - 4.5f, kPanY - 5.0f, 9.0f, 10.0f, 1.5f);
}

void LfoDisplay::setPanFromX (float x)
{
    const float pan = juce::jlimit (-1.0f, 1.0f, ((x - kPanLeft) / (kPanRight - kPanLeft)) * 2.0f - 1.0f);
    if (auto* p = proc.getState().getParameter (ids::pan))
        p->setValueNotifyingHost (p->convertTo0to1 (std::round (pan * 100.0f)));
}

void LfoDisplay::mouseDown (const juce::MouseEvent& e)
{
    draggingPan = ! mono && std::abs (e.position.y - kPanY) < 22.0f && e.position.x > kPanLeft - 30.0f && e.position.x < kPanRight + 30.0f;
    if (! draggingPan)
        return;
    if (auto* p = proc.getState().getParameter (ids::pan))
        p->beginChangeGesture();
    setPanFromX (e.position.x);
}

void LfoDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingPan)
        setPanFromX (e.position.x);
}

void LfoDisplay::mouseUp (const juce::MouseEvent&)
{
    if (! draggingPan)
        return;
    if (auto* p = proc.getState().getParameter (ids::pan))
        p->endChangeGesture();
    draggingPan = false;
}

void LfoDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (mono || std::abs (e.position.y - kPanY) > 22.0f)
        return;
    if (auto* p = proc.getState().getParameter (ids::pan))
    {
        const bool inGesture = draggingPan; // el segundo clic ya abrió el gesto
        if (! inGesture)
            p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        if (! inGesture)
            p->endChangeGesture();
    }
}

} // namespace labd::ui
