#include "ui/EqDisplay.h"

namespace labd::ui {

namespace {
constexpr double kMinHz = 20.0, kMaxHz = 20000.0;
constexpr double kTopDb = 18.0, kBottomDb = -24.0;
constexpr int kCurvePoints = 300;

bool isCut (FilterType t) { return t == FilterType::LowCut || t == FilterType::HighCut; }
} // namespace

EqDisplay::EqDisplay (LabdProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
    refresh();
}

juce::Rectangle<float> EqDisplay::plotArea() const noexcept
{
    // Frontal: 20 Hz en x=890, 20 kHz en x=1705, +18 dB en y=328, -24 dB en y=672.
    return { 71.0f, 49.0f, 815.0f, 344.0f };
}

float EqDisplay::xForFreq (double hz) const noexcept
{
    const auto a = plotArea();
    return a.getX() + (float) (std::log (hz / kMinHz) / std::log (kMaxHz / kMinHz)) * a.getWidth();
}

double EqDisplay::freqForX (float x) const noexcept
{
    const auto a = plotArea();
    return kMinHz * std::pow (kMaxHz / kMinHz, juce::jlimit (0.0, 1.0, (double) ((x - a.getX()) / a.getWidth())));
}

float EqDisplay::yForDb (double db) const noexcept
{
    const auto a = plotArea();
    return a.getY() + (float) ((kTopDb - juce::jlimit (kBottomDb, kTopDb, db)) / (kTopDb - kBottomDb)) * a.getHeight();
}

double EqDisplay::dbForY (float y) const noexcept
{
    const auto a = plotArea();
    return kTopDb - (double) ((y - a.getY()) / a.getHeight()) * (kTopDb - kBottomDb);
}

void EqDisplay::refresh()
{
    auto& s = proc.getState();
    auto v = [&] (const juce::String& id) { return s.getRawParameterValue (id)->load(); };
    const auto& tl = proc.getTelemetry();
    sampleRate = proc.getSampleRate() > 0.0 ? proc.getSampleRate() : 48000.0;
    eqOn = v (ids::eqOn) >= 0.5f;
    bypassed = v (ids::bypass) >= 0.5f;
    const bool shelf6 = juce::roundToInt (v (ids::eq6Type)) == 1;
    const bool lfoOn = v (ids::lfoOn) >= 0.5f;
    modulating = false;
    for (int b = 0; b < kNumEqBands; ++b)
    {
        auto& bv = bands[(size_t) b];
        bv.on = v (ids::eqOnId (b)) >= 0.5f;
        bv.freq = v (ids::eqFreqId (b));
        bv.gain = ids::bandHasGain (b) ? v (ids::eqGainId (b)) : 0.0;
        bv.q = ids::bandHasQ (b) ? v (ids::eqQId (b)) : kShelfQ;
        bv.type = bandFilterType (b, shelf6);
        bv.depth = v (ids::lfoDepthId (b)) * 0.01f;
        const double eff = tl.eqEffectiveFreq[(size_t) b].load();
        bv.effectiveFreq = eff > 0.0 ? eff : bv.freq;
        if (eqOn && bv.on && lfoOn && bv.depth > 0.0f && tl.lfoGate.load() > 0.0f)
            modulating = true;
    }
    selected = proc.getSelectedBand();
    repaint();
}

double EqDisplay::responseDbAt (double hz, bool modulated) const
{
    if (! eqOn)
        return 0.0;
    std::complex<double> h = 1.0;
    for (const auto& b : bands)
    {
        if (! b.on)
            continue;
        const double f = modulated ? b.effectiveFreq : b.freq;
        h *= svfResponse (designSvf (b.type, f, b.gain, b.q, sampleRate), hz, sampleRate);
    }
    return 20.0 * std::log10 (std::max (std::abs (h), 1.0e-9));
}

juce::Path EqDisplay::buildCurve (bool modulated, bool closeForFill) const
{
    juce::Path p;
    const auto a = plotArea();
    for (int i = 0; i < kCurvePoints; ++i)
    {
        const double hz = kMinHz * std::pow (kMaxHz / kMinHz, (double) i / (kCurvePoints - 1));
        const float x = a.getX() + a.getWidth() * (float) i / (kCurvePoints - 1);
        const float y = yForDb (hz < 0.5 * sampleRate ? responseDbAt (hz, modulated) : kBottomDb);
        if (i == 0)
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
    }
    if (closeForFill)
    {
        p.lineTo (a.getRight(), a.getBottom());
        p.lineTo (a.getX(), a.getBottom());
        p.closeSubPath();
    }
    return p;
}

juce::Point<float> EqDisplay::nodePosition (int b) const
{
    const auto& bv = bands[(size_t) b];
    const double y = isCut (bv.type) ? 20.0 * std::log10 (bv.q) : bv.gain;
    return { xForFreq (clampEqFreq (bv.freq, sampleRate)), yForDb (y) };
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    // Pantalla
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

    const auto a = plotArea();
    const bool dim = bypassed || ! eqOn;

    // Retícula
    g.setFont (valueFont (19.5f));
    for (double db : { 18.0, 12.0, 6.0, 0.0, -6.0, -12.0, -18.0, -24.0 })
    {
        const float y = yForDb (db);
        g.setColour (db == 0.0 ? juce::Colour (0x669dbc76) : colours::grid);
        g.drawHorizontalLine (juce::roundToInt (y), a.getX(), a.getRight());
        if (db == 18.0 || db == 12.0 || db == 0.0 || db == -12.0 || db == -24.0)
        {
            g.setColour (colours::cream.withAlpha (0.85f));
            const auto label = db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db);
            g.drawText (label, juce::Rectangle<float> (a.getX() - 66.0f, y - 11.0f, 56.0f, 22.0f), juce::Justification::centredRight);
        }
    }
    g.setColour (colours::cream.withAlpha (0.85f));
    g.drawText ("dB", juce::Rectangle<float> (28.0f, 14.0f, 40.0f, 22.0f), juce::Justification::centredLeft);
    for (double hz : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const float x = xForFreq (hz);
        g.setColour (colours::grid);
        g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());
        if (hz == 20.0 || hz == 100.0 || hz == 500.0 || hz == 1000.0 || hz == 5000.0 || hz == 20000.0)
        {
            g.setColour (colours::cream.withAlpha (0.9f));
            const auto label = hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz);
            g.setFont (valueFont (21.0f));
            g.drawText (label, juce::Rectangle<float> (x - 40.0f, a.getBottom() + 12.0f, 80.0f, 26.0f), juce::Justification::centred);
            g.setFont (valueFont (19.5f));
        }
    }

    // Curvas
    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (a.expanded (2.0f, 2.0f).toNearestInt());
        const auto fill = buildCurve (false, true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a5a34).withAlpha (dim ? 0.35f : 0.85f), 0, a.getY(),
                                                 juce::Colour (0xff223a22).withAlpha (dim ? 0.30f : 0.80f), 0, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colours::curve.withAlpha (dim ? 0.45f : 1.0f));
        g.strokePath (buildCurve (false, false), juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (modulating && ! bypassed)
        {
            g.setColour (juce::Colour (0xffefbf78).withAlpha (0.85f));
            g.strokePath (buildCurve (true, false), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Nodos
    for (int b = 0; b < kNumEqBands; ++b)
    {
        const auto& bv = bands[(size_t) b];
        const auto pos = nodePosition (b);
        const bool sel = b == selected;
        const float rad = sel ? 21.0f : 16.5f;
        const bool active = bv.on && eqOn && ! bypassed;
        if (modulating && active && bv.depth > 0.0f)
        {
            g.setColour (juce::Colour (0xffeab775).withAlpha (0.9f));
            const float mx = xForFreq (bv.effectiveFreq);
            g.drawEllipse (mx - 7.0f, pos.y + (sel ? 0.0f : 0.0f) - 7.0f + 0.0f, 14.0f, 14.0f, 2.0f);
        }
        g.setColour (sel ? juce::Colour (0xffd3e7aa) : juce::Colour (0xff1c2a1a));
        g.fillEllipse (pos.x - rad, pos.y - rad, 2 * rad, 2 * rad);
        g.setColour (active ? juce::Colour (0xffb5ce8a) : juce::Colour (0xff67745e));
        g.drawEllipse (pos.x - rad, pos.y - rad, 2 * rad, 2 * rad, sel ? 2.6f : 2.0f);
        g.setColour (sel ? juce::Colour (0xff172015) : colours::cream.withAlpha (active ? 1.0f : 0.55f));
        g.setFont (valueFont (sel ? 22.0f : 20.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (pos.x - rad, pos.y - rad, 2 * rad, 2 * rad), juce::Justification::centred);
    }
}

void EqDisplay::setParam (const juce::String& id, float value)
{
    if (auto* p = proc.getState().getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = -1;
    float best = 44.0f;
    for (int b = 0; b < kNumEqBands; ++b)
    {
        const float d = nodePosition (b).getDistanceFrom (e.position);
        if (d < best)
        {
            best = d;
            dragging = b;
        }
    }
    if (dragging < 0)
        return;
    if (onBandSelected)
        onBandSelected (dragging);
    proc.getUndoManager().beginNewTransaction();
    if (auto* p = proc.getState().getParameter (ids::eqFreqId (dragging)))
        p->beginChangeGesture();
    if (! isCut (bands[(size_t) dragging].type))
        if (auto* p = proc.getState().getParameter (ids::eqGainId (dragging)))
            p->beginChangeGesture();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    setParam (ids::eqFreqId (dragging), (float) freqForX (e.position.x));
    if (! isCut (bands[(size_t) dragging].type) && ids::bandHasGain (dragging))
        setParam (ids::eqGainId (dragging), (float) juce::jlimit (-18.0, 18.0, std::round (dbForY (e.position.y) * 10.0) / 10.0));
    refresh();
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging < 0)
        return;
    if (auto* p = proc.getState().getParameter (ids::eqFreqId (dragging)))
        p->endChangeGesture();
    if (! isCut (bands[(size_t) dragging].type))
        if (auto* p = proc.getState().getParameter (ids::eqGainId (dragging)))
            p->endChangeGesture();
    dragging = -1;
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Doble clic sobre un nodo: GAIN a 0 dB (bandas con ganancia).
    for (int b = 0; b < kNumEqBands; ++b)
        if (nodePosition (b).getDistanceFrom (e.position) < 30.0f && ids::bandHasGain (b) && ! isCut (bands[(size_t) b].type))
        {
            proc.getUndoManager().beginNewTransaction();
            if (auto* p = proc.getState().getParameter (ids::eqGainId (b)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
                p->endChangeGesture();
            }
        }
}

} // namespace labd::ui
