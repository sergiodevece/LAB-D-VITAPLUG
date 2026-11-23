#include "ui/PluginEditor.h"

#include "LabdAssets.h"

namespace labd {

using namespace ui;

namespace {
// --- Geometría del frontal (px del PNG aprobado, 2640 x 1200) -----------------------
juce::Rectangle<int> knobBounds (int cx, int cy, int r) { return { cx - r, cy - r, 2 * r, 2 * r }; }

constexpr int kFaderX[kNumLfoDepths] = { 1864, 1965, 2067, 2169, 2270, 2372, 2474 };
constexpr int kBandKeyX[kNumEqBands] = { 825, 935, 1045, 1155, 1265, 1375 };
const char* const kShapeNames[5] = { "SIN", "TRI", "SAW", "SQR", "RND" };
constexpr int kShapeX[5] = { 1820, 1962, 2102, 2244, 2385 };
constexpr int kShapeW[5] = { 140, 138, 140, 139, 140 };

const juce::Rectangle<int> kFooterReadout { 2250, 1130, 270, 42 };
const juce::Rectangle<int> kLayoutReadout { 1890, 1130, 340, 42 };

juce::String typeLabel (int band, bool shelf6)
{
    switch (band)
    {
        case 0: return "LOW CUT";
        case 1: return "LOW SHELF";
        case 5: return shelf6 ? "HIGH SHELF" : "HIGH CUT"; // conmutable solo en la banda 6
        default: return "BELL";
    }
}
} // namespace

LabdEditor::LabdEditor (LabdProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p), state (p.getState())
{
    setLookAndFeel (&lnf);
    background = juce::ImageCache::getFromMemory (LabdAssets::LABD_Frontal_png, LabdAssets::LABD_Frontal_pngSize);

    addAndMakeVisible (panel);
    panel.setBounds (0, 0, kDesignWidth, kDesignHeight);
    panel.addMouseListener (this, true);
    buildControls();

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (kMinWidth, kMinHeight, kDesignWidth, kDesignHeight);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) kDesignWidth / (double) kDesignHeight);
    setSize (kNominalWidth, kNominalHeight);

    timerCallback();
    startTimerHz (30);
}

LabdEditor::~LabdEditor()
{
    stopTimer();
    proc.getEditHistory().closeAllGroups();
    panel.removeMouseListener (this);
    setLookAndFeel (nullptr);
}

float LabdEditor::raw (const juce::String& id) const
{
    return state.getRawParameterValue (id)->load();
}

void LabdEditor::setChoice (const juce::String& id, int index)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void LabdEditor::setBool (const juce::String& id, bool value) { setChoice (id, value ? 1 : 0); }

void LabdEditor::bindTextEntry (ValueDisplay& display, std::function<juce::String()> paramId)
{
    display.onTextEntered = [this, paramId] (const juce::String& text) {
        if (auto* p = state.getParameter (paramId()))
        {
            auto t = text.trim();
            float v = p->getValueForText (t);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
            {
                const int idx = choice->choices.indexOf (t.toUpperCase(), true);
                if (idx < 0)
                    return; // texto no válido: no cambia nada
                v = p->convertTo0to1 ((float) idx);
            }
            else if (! isNumericText (t))
            {
                return; // p. ej. "1/4" con TIME en ms: no cambia nada
            }
            p->beginChangeGesture();
            p->setValueNotifyingHost (v);
            p->endChangeGesture();
        }
    };
    display.textForEditing = [this, paramId] {
        if (auto* p = state.getParameter (paramId()))
            return p->getCurrentValueAsText();
        return juce::String();
    };
}

void LabdEditor::buildControls()
{
    auto addKnob = [&] (Knob& k, juce::Rectangle<int> b) {
        panel.addAndMakeVisible (k);
        k.setBounds (b);
    };
    auto addDisplay = [&] (ValueDisplay& d, juce::Rectangle<int> b) {
        panel.addAndMakeVisible (d);
        d.setBounds (b);
    };
    auto addButton = [&] (juce::Button& btn, juce::Rectangle<int> b) {
        panel.addAndMakeVisible (btn);
        btn.setBounds (b);
    };

    // --- DELAY ---------------------------------------------------------------------
    addKnob (timeL, knobBounds (268, 420, 84));
    addKnob (timeR, knobBounds (592, 420, 84));
    addDisplay (timeLDisplay, { 184, 540, 168, 84 });
    addDisplay (timeRDisplay, { 508, 540, 168, 84 });
    bindTextEntry (timeLDisplay, [this] { return timeLParam; });
    bindTextEntry (timeRDisplay, [this] { return timeRParam; });
    addButton (syncButton, { 502, 200, 116, 53 });
    addButton (linkButton, { 630, 200, 118, 53 });
    syncButton.setClickingTogglesState (true);
    linkButton.setClickingTogglesState (true);
    syncAtt = std::make_unique<ButtonAttachment> (state, ids::timeSync, syncButton);
    linkAtt = std::make_unique<ButtonAttachment> (state, ids::timeLink, linkButton);

    addKnob (feedbackKnob, knobBounds (216, 775, 48));
    addKnob (mixKnob, knobBounds (432, 775, 48));
    addKnob (outputKnob, knobBounds (646, 775, 48));
    feedbackAtt = std::make_unique<SliderAttachment> (state, ids::feedback, feedbackKnob);
    mixAtt = std::make_unique<SliderAttachment> (state, ids::mix, mixKnob);
    outputAtt = std::make_unique<SliderAttachment> (state, ids::output, outputKnob);
    feedbackKnob.defaultValue = 30.0;
    mixKnob.defaultValue = 25.0;
    outputKnob.defaultValue = 0.0;
    addDisplay (feedbackDisplay, { 150, 851, 132, 36 });
    addDisplay (mixDisplay, { 366, 851, 132, 36 });
    addDisplay (outputDisplay, { 580, 851, 132, 36 });
    bindTextEntry (feedbackDisplay, [] { return juce::String (ids::feedback); });
    bindTextEntry (mixDisplay, [] { return juce::String (ids::mix); });
    bindTextEntry (outputDisplay, [] { return juce::String (ids::output); });

    addButton (stereoButton, { 118, 933, 123, 52 });
    addButton (pingPongButton, { 243, 933, 187, 52 });
    addButton (cleanButton, { 448, 933, 149, 52 });
    addButton (tapeButton, { 599, 933, 149, 52 });
    stereoButton.onClick = [this] { setChoice (ids::stereoMode, 0); };
    pingPongButton.onClick = [this] { setChoice (ids::stereoMode, 1); };
    cleanButton.onClick = [this] { setChoice (ids::color, 0); };
    tapeButton.onClick = [this] { setChoice (ids::color, 1); };

    panel.addAndMakeVisible (driveSlider);
    panel.addAndMakeVisible (duckSlider);
    driveSlider.setBounds (117, 1038, 298, 24);
    duckSlider.setBounds (451, 1038, 298, 24);
    driveAtt = std::make_unique<SliderAttachment> (state, ids::drive, driveSlider);
    duckAtt = std::make_unique<SliderAttachment> (state, ids::duck, duckSlider);
    for (auto* s : { &driveSlider, &duckSlider })
        s->setDoubleClickReturnValue (true, 0.0);
    addDisplay (driveValue, { 340, 1006, 76, 30 });
    addDisplay (duckValue, { 672, 1006, 76, 30 });
    bindTextEntry (driveValue, [] { return juce::String (ids::drive); });
    bindTextEntry (duckValue, [] { return juce::String (ids::duck); });

    // --- ECHO EQ -------------------------------------------------------------------------
    panel.addAndMakeVisible (eqDisplay);
    eqDisplay.setBounds (819, 279, 928, 444);
    eqDisplay.onBandSelected = [this] (int b) { selectBand (b); };
    addButton (eqInButton, { 1621, 201, 120, 52 });
    eqInButton.setClickingTogglesState (true);
    eqInAtt = std::make_unique<ButtonAttachment> (state, ids::eqOn, eqInButton);
    for (int b = 0; b < kNumEqBands; ++b)
    {
        bandKeys[(size_t) b] = std::make_unique<PanelButton> (juce::String (b + 1), PanelButton::Kind::Band);
        addButton (*bandKeys[(size_t) b], { kBandKeyX[b], 747, 98, 53 });
        bandKeys[(size_t) b]->onClick = [this, b] { selectBand (b); };
    }
    addButton (typeButton, { 1489, 747, 148, 53 });
    typeButton.onClick = [this] {
        if (proc.getSelectedBand() == 5)
            setChoice (ids::eq6Type, juce::roundToInt (raw (ids::eq6Type)) == 1 ? 0 : 1);
    };
    addButton (bandInButton, { 1651, 747, 90, 53 });
    bandInButton.setClickingTogglesState (true);
    addKnob (freqKnob, knobBounds (976, 913, 48));
    addKnob (gainKnob, knobBounds (1280, 913, 48));
    addKnob (qKnob, knobBounds (1588, 913, 48));
    addDisplay (freqDisplay, { 910, 987, 132, 37 });
    addDisplay (gainDisplay, { 1214, 987, 132, 37 });
    addDisplay (qDisplay, { 1522, 987, 132, 37 });
    bindTextEntry (freqDisplay, [this] { return freqParam; });
    bindTextEntry (gainDisplay, [this] { return gainParam; });
    bindTextEntry (qDisplay, [this] { return qParam; });

    // --- LFO -------------------------------------------------------------------------------
    panel.addAndMakeVisible (lfoDisplay);
    lfoDisplay.setBounds (1813, 279, 717, 232);
    addButton (autoButton, { 2178, 201, 109, 52 });
    addButton (manualButton, { 2289, 201, 128, 52 });
    addButton (lfoOnButton, { 2435, 201, 88, 52 });
    autoButton.onClick = [this] { setChoice (ids::lfoMode, 1); };
    manualButton.onClick = [this] { setChoice (ids::lfoMode, 0); };
    lfoOnButton.setClickingTogglesState (true);
    lfoOnAtt = std::make_unique<ButtonAttachment> (state, ids::lfoOn, lfoOnButton);
    for (int i = 0; i < 5; ++i)
    {
        shapeButtons[(size_t) i] = std::make_unique<PanelButton> (kShapeNames[i], PanelButton::Kind::Segment);
        addButton (*shapeButtons[(size_t) i], { kShapeX[i], 529, kShapeW[i], 52 });
        shapeButtons[(size_t) i]->onClick = [this, i] { setChoice (ids::lfoShape, i); };
    }
    addKnob (rateKnob, knobBounds (1958, 698, 46));
    addKnob (phaseKnob, knobBounds (2246, 698, 46));
    phaseAtt = std::make_unique<SliderAttachment> (state, ids::lfoPhase, phaseKnob);
    phaseKnob.defaultValue = 0.0;
    addDisplay (rateDisplay, { 1892, 770, 132, 37 });
    addDisplay (phaseDisplay, { 2180, 770, 132, 37 });
    bindTextEntry (rateDisplay, [this] { return rateParam; });
    bindTextEntry (phaseDisplay, [] { return juce::String (ids::lfoPhase); });
    addButton (rateSyncButton, { 2409, 608, 115, 53 });
    rateSyncButton.setClickingTogglesState (true);
    rateSyncAtt = std::make_unique<ButtonAttachment> (state, ids::lfoRateSync, rateSyncButton);

    for (int i = 0; i < kNumLfoDepths; ++i)
    {
        auto& f = depthFaders[(size_t) i];
        f = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
        panel.addAndMakeVisible (*f);
        f->setBounds (kFaderX[i] - 17, 942, 34, 105);
        depthAtts[(size_t) i] = std::make_unique<SliderAttachment> (state, ids::lfoDepthId (i), *f);
        f->setDoubleClickReturnValue (true, 0.0);
        auto& v = depthValues[(size_t) i];
        v = std::make_unique<ValueDisplay> (ValueDisplay::Style::Plain);
        addDisplay (*v, { kFaderX[i] - 40, 916, 80, 28 });
        bindTextEntry (*v, [i] { return ids::lfoDepthId (i); });
    }

    // --- Pie ---------------------------------------------------------------------------------
    addButton (bypassButton, { 133, 1125, 157, 53 });
    addButton (wetOnlyButton, { 303, 1125, 174, 53 });
    bypassButton.setClickingTogglesState (true);
    wetOnlyButton.setClickingTogglesState (true);
    bypassAtt = std::make_unique<ButtonAttachment> (state, ids::bypass, bypassButton);
    wetOnlyAtt = std::make_unique<ButtonAttachment> (state, ids::wetOnly, wetOnlyButton);

    // Identificadores para pruebas y accesibilidad.
    timeL.setComponentID ("timeL");
    timeR.setComponentID ("timeR");
    mixKnob.setComponentID ("mix");
    pingPongButton.setComponentID ("pingPong");
    freqKnob.setComponentID ("freq");
    gainKnob.setComponentID ("gain");
    qKnob.setComponentID ("q");
    rateKnob.setComponentID ("rate");
    typeButton.setComponentID ("type");
    eqDisplay.setComponentID ("eqDisplay");
    lfoDisplay.setComponentID ("lfoDisplay");
    timeLDisplay.setComponentID ("timeLDisplay");
    timeRDisplay.setComponentID ("timeRDisplay");
    mixDisplay.setComponentID ("mixDisplay");
    gainDisplay.setComponentID ("gainDisplay");
    qDisplay.setComponentID ("qDisplay");
    rateDisplay.setComponentID ("rateDisplay");
    timeL.setTitle ("Time L");
    timeR.setTitle ("Time R");
    feedbackKnob.setTitle ("Feedback");
    mixKnob.setTitle ("Mix");
    outputKnob.setTitle ("Output");
    freqKnob.setTitle ("EQ Freq");
    gainKnob.setTitle ("EQ Gain");
    qKnob.setTitle ("EQ Q");
    rateKnob.setTitle ("LFO Rate");
    phaseKnob.setTitle ("LFO Phase");
}

juce::Component* LabdEditor::findById (const juce::String& id) const
{
    for (auto* c : panel.getChildren())
        if (c->getComponentID() == id)
            return c;
    return nullptr;
}

void LabdEditor::selectBand (int band)
{
    proc.setSelectedBand (band);
    timerCallback();
}

void LabdEditor::attachTimeKnobs (bool sync, bool link)
{
    const int mode = (sync ? 1 : 0) + (link ? 2 : 0);
    if (mode == attachedTimeMode)
        return;
    // Nunca se cambia el parámetro de un mando a mitad de un arrastre: el gesto
    // abierto debe cerrarse en el mismo parámetro (se reintenta al soltar).
    if (timeL.isMouseButtonDown() || timeR.isMouseButtonDown())
        return;
    attachedTimeMode = mode;
    timeLAtt.reset();
    timeRAtt.reset();
    timeLParam = sync ? ids::timeLNote : ids::timeLMs;
    // LINK: los dos mandos editan el tiempo común (parámetros de L).
    timeRParam = link ? timeLParam : juce::String (sync ? ids::timeRNote : ids::timeRMs);
    timeLAtt = std::make_unique<SliderAttachment> (state, timeLParam, timeL);
    timeRAtt = std::make_unique<SliderAttachment> (state, timeRParam, timeR);
    auto def = [this] (const juce::String& id) {
        auto* p = state.getParameter (id);
        return (double) p->convertFrom0to1 (p->getDefaultValue());
    };
    timeL.defaultValue = def (timeLParam);
    timeR.defaultValue = def (timeRParam);
}

void LabdEditor::attachEqKnobs (int band, bool shelf6)
{
    if (band == attachedBand && (int) shelf6 == attachedShelf)
        return;
    if (freqKnob.isMouseButtonDown() || gainKnob.isMouseButtonDown() || qKnob.isMouseButtonDown())
        return;
    attachedBand = band;
    attachedShelf = (int) shelf6;
    freqAtt.reset();
    gainAtt.reset();
    qAtt.reset();
    bandInAtt.reset();
    freqParam = ids::eqFreqId (band);
    gainParam = ids::bandHasGain (band) ? ids::eqGainId (band) : juce::String();
    qParam = ids::bandHasQ (band) ? ids::eqQId (band) : juce::String();
    freqAtt = std::make_unique<SliderAttachment> (state, freqParam, freqKnob);
    freqKnob.defaultValue = kInitBands[band].freq;
    if (gainParam.isNotEmpty())
    {
        gainAtt = std::make_unique<SliderAttachment> (state, gainParam, gainKnob);
        gainKnob.defaultValue = 0.0;
    }
    if (qParam.isNotEmpty())
    {
        qAtt = std::make_unique<SliderAttachment> (state, qParam, qKnob);
        qKnob.defaultValue = kInitBands[band].q;
    }
    bandInAtt = std::make_unique<ButtonAttachment> (state, ids::eqOnId (band), bandInButton);
}

void LabdEditor::attachRateKnob (bool sync)
{
    if ((int) sync == attachedRateSync)
        return;
    if (rateKnob.isMouseButtonDown())
        return;
    attachedRateSync = (int) sync;
    rateAtt.reset();
    rateParam = sync ? ids::lfoRateNote : ids::lfoRate;
    rateAtt = std::make_unique<SliderAttachment> (state, rateParam, rateKnob);
    auto* p = state.getParameter (rateParam);
    rateKnob.defaultValue = p->convertFrom0to1 (p->getDefaultValue());
}

void LabdEditor::timerCallback()
{
    proc.getEditHistory().service();
    const auto& tl = proc.getTelemetry();
    const bool sync = raw (ids::timeSync) >= 0.5f;
    const bool link = raw (ids::timeLink) >= 0.5f;
    const bool mono = tl.outputChannels.load() < 2;
    const double bpm = tl.bpm.load();

    // --- DELAY --------------------------------------------------------------------
    attachTimeKnobs (sync, link);
    auto timeText = [&] (bool left, juce::String& main, juce::String& sub) {
        const bool useL = left || link;
        if (sync)
        {
            const int idx = juce::roundToInt (raw (useL ? ids::timeLNote : ids::timeRNote));
            main = tempo::kNotes[juce::jlimit (0, tempo::kNumNotes - 1, idx)].label;
            sub = format::timeMs (tempo::noteMs (idx, bpm));
        }
        else
        {
            main = format::timeMs (raw (useL ? ids::timeLMs : ids::timeRMs));
            sub = "FREE";
        }
        if (! left && link)
            sub = "LINK " + sub;
    };
    juce::String m, s;
    timeText (true, m, s);
    timeLDisplay.setText (m, s);
    if (mono)
    {
        timeRDisplay.setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), "MONO: TIME L");
        timeR.setEnabled (false);
        timeRDisplay.setEnabled (false);
    }
    else
    {
        timeText (false, m, s);
        timeRDisplay.setText (m, s);
        timeR.setEnabled (true);
        timeRDisplay.setEnabled (true);
    }
    syncButton.setLit (sync);
    linkButton.setLit (link);

    feedbackDisplay.setText (format::percent (raw (ids::feedback)));
    const bool wetOnly = raw (ids::wetOnly) >= 0.5f;
    if (mixKnob.isOverridden() != wetOnly)
    {
        mixKnob.setOverride (wetOnly, 1.0);
        mixKnob.repaint();
    }
    mixKnob.setEnabled (! wetOnly);
    mixDisplay.setText (wetOnly ? juce::String ("100%") : format::percent (raw (ids::mix)));
    mixDisplay.setColour (wetOnly ? colours::amber : colours::cream);
    outputDisplay.setText (format::decibels (raw (ids::output)));

    const bool pp = juce::roundToInt (raw (ids::stereoMode)) == 1;
    stereoButton.setLit (! pp);
    pingPongButton.setLit (pp);
    pingPongButton.setDimmed (mono); // mono → mono: realimentación mono en ambos modos
    const bool tape = juce::roundToInt (raw (ids::color)) == 1;
    cleanButton.setLit (! tape);
    tapeButton.setLit (tape);
    driveValue.setText (format::percent (raw (ids::drive)));
    duckValue.setText (format::percent (raw (ids::duck)));
    driveValue.setColour (raw (ids::drive) > 0.0f ? colours::amber : juce::Colour (0xffb3b09c));
    duckValue.setColour (raw (ids::duck) > 0.0f ? colours::amber : juce::Colour (0xffb3b09c));

    // --- ECHO EQ ----------------------------------------------------------------------
    const int band = proc.getSelectedBand();
    const bool shelf6 = juce::roundToInt (raw (ids::eq6Type)) == 1;
    attachEqKnobs (band, shelf6);
    const auto type = bandFilterType (band, shelf6);
    const bool cut = type == FilterType::LowCut || type == FilterType::HighCut;
    const bool shelf = type == FilterType::LowShelf || type == FilterType::HighShelf;
    eqInButton.setLit (raw (ids::eqOn) >= 0.5f);
    for (int b = 0; b < kNumEqBands; ++b)
    {
        bandKeys[(size_t) b]->setLit (b == band);
        bandKeys[(size_t) b]->setMarker (raw (ids::lfoDepthId (b)) > 0.0f);
    }
    typeButton.setLabel (typeLabel (band, shelf6));
    typeButton.setEnabled (band == 5);
    typeButton.setLit (false);
    bandInButton.setLit (raw (ids::eqOnId (band)) >= 0.5f);
    freqDisplay.setText (format::frequency (raw (freqParam)));
    const bool gainActive = gainParam.isNotEmpty() && ! cut;
    gainKnob.setEnabled (gainActive);
    gainKnob.setOverride (! gainActive, 0.5);
    gainDisplay.setEnabled (gainActive);
    gainDisplay.setText (gainActive ? format::decibels (raw (gainParam)) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")));
    const bool qActive = qParam.isNotEmpty() && ! shelf;
    qKnob.setEnabled (qActive);
    qKnob.setOverride (! qActive, 0.5);
    qDisplay.setEnabled (qActive);
    qDisplay.setText (qActive ? format::q (raw (qParam)) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")));
    eqDisplay.refresh();

    // --- LFO ------------------------------------------------------------------------------
    const bool autoMode = juce::roundToInt (raw (ids::lfoMode)) == 1;
    const bool rateSync = raw (ids::lfoRateSync) >= 0.5f;
    autoButton.setLit (autoMode);
    manualButton.setLit (! autoMode);
    lfoOnButton.setLit (raw (ids::lfoOn) >= 0.5f);
    const int shapeIdx = autoMode ? 0 : juce::roundToInt (raw (ids::lfoShape));
    for (int i = 0; i < 5; ++i)
    {
        shapeButtons[(size_t) i]->setLit (i == shapeIdx);
        shapeButtons[(size_t) i]->setEnabled (! autoMode);
    }
    attachRateKnob (rateSync);
    rateKnob.setEnabled (! autoMode);
    rateKnob.setOverride (autoMode, (double) (tempo::rateIndex ("1 BAR")) / (tempo::kNumRates - 1));
    rateDisplay.setEnabled (! autoMode);
    if (autoMode)
        rateDisplay.setText ("1 BAR");
    else if (rateSync)
        rateDisplay.setText (tempo::kRates[juce::jlimit (0, tempo::kNumRates - 1, juce::roundToInt (raw (ids::lfoRateNote)))].label);
    else
        rateDisplay.setText (format::hz (raw (ids::lfoRate)));
    phaseKnob.setEnabled (! autoMode);
    phaseKnob.setOverride (autoMode, 0.0);
    phaseDisplay.setEnabled (! autoMode);
    phaseDisplay.setText (juce::String (juce::roundToInt (autoMode ? 0.0f : raw (ids::lfoPhase))) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")));
    rateSyncButton.setLit (rateSync || autoMode);
    rateSyncButton.setEnabled (! autoMode);
    for (int i = 0; i < kNumLfoDepths; ++i)
    {
        const float d = raw (ids::lfoDepthId (i));
        depthValues[(size_t) i]->setText (format::percent (d));
        depthValues[(size_t) i]->setColour (d > 0.0f ? colours::amber : juce::Colour (0xffb3b09c));
    }
    depthFaders[kPanDepthIndex]->setEnabled (! mono);
    lfoDisplay.refresh();

    // --- Pie ---------------------------------------------------------------------------------
    bypassButton.setLit (raw (ids::bypass) >= 0.5f);
    wetOnlyButton.setLit (wetOnly);
    const auto newBpm = juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.05 ? 0 : 1) + " BPM";
    const bool host = tl.bpmFromHost.load();
    const int inCh = tl.inputChannels.load(), outCh = tl.outputChannels.load();
    const auto newLayout = inCh == 1 && outCh == 1 ? juce::String ("MONO")
                           : inCh == 1 && outCh == 2 ? juce::String (juce::CharPointer_UTF8 ("MONO \xe2\x86\x92 STEREO"))
                                                     : juce::String ("STEREO");
    if (newBpm != bpmText || host != bpmFromHost || newLayout != layoutText)
    {
        bpmText = newBpm;
        bpmFromHost = host;
        layoutText = newLayout;
        panel.repaint (kFooterReadout.getUnion (kLayoutReadout));
    }
}

void LabdEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1d2419));
}

void LabdEditor::resized()
{
    const float scale = (float) getWidth() / (float) kDesignWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
}

namespace {
// Rellena dest con copias de src del propio frontal (tapa el arte fijo de
// elementos que ahora son dinámicos con textura de chasis limpia).
void tile (juce::Graphics& g, const juce::Image& img, juce::Rectangle<int> dest, juce::Rectangle<int> src)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (dest);
    for (int y = dest.getY(); y < dest.getBottom(); y += src.getHeight())
        for (int x = dest.getX(); x < dest.getRight(); x += src.getWidth())
            g.drawImage (img, x, y, src.getWidth(), src.getHeight(), src.getX(), src.getY(), src.getWidth(), src.getHeight());
}
} // namespace

void LabdEditor::paintPatches (juce::Graphics& g)
{
    // Línea secundaria de TIME L/R ("375 ms", "500 ms").
    tile (g, background, { 180, 597, 176, 30 }, { 380, 597, 110, 30 });
    tile (g, background, { 504, 597, 176, 30 }, { 380, 597, 110, 30 });
    // DRIVE / DUCK: lectura y deslizador.
    tile (g, background, { 336, 1004, 84, 32 }, { 230, 1004, 100, 32 });
    tile (g, background, { 668, 1004, 84, 32 }, { 540, 1004, 100, 32 });
    tile (g, background, { 108, 1034, 316, 32 }, { 108, 1064, 316, 32 });
    tile (g, background, { 442, 1034, 316, 32 }, { 442, 1064, 316, 32 });
    // Faders de profundidad y sus cantidades.
    for (int i = 0; i < kNumLfoDepths; ++i)
    {
        const int c = kFaderX[i];
        if (i == kPanDepthIndex) // PAN: textura del borde derecho, sin la divisoria
            tile (g, background, { c - 31, 912, 60, 142 }, { 2489, 912, 34, 142 });
        else
            tile (g, background, { c - 26, 912, 52, 142 }, { i <= 4 ? c - 26 + 50 : c - 26 - 50, 912, 52, 142 });
    }
    // Lectura del pie (HOST / BPM).
    tile (g, background, kFooterReadout, { 1975, kFooterReadout.getY(), kFooterReadout.getWidth(), kFooterReadout.getHeight() });
}

void LabdEditor::Panel::paint (juce::Graphics& g)
{
    auto& e = owner;
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    if (e.background.isValid())
        g.drawImageAt (e.background, 0, 0);
    else
        g.fillAll (juce::Colour (0xff3a4a32));
    e.paintPatches (g);

    // Pie: LED HOST + BPM (o INT cuando se usa el último BPM válido o la reserva).
    const auto r = kFooterReadout.toFloat();
    drawLed (g, { r.getX() + 18.0f, r.getCentreY() }, 6.0f, e.bpmFromHost);
    g.setColour (colours::cream.withAlpha (0.9f));
    g.setFont (labelFont (19.0f));
    g.drawText (e.bpmFromHost ? "HOST" : "INT", juce::Rectangle<float> (r.getX() + 38.0f, r.getY(), 80.0f, r.getHeight()),
                juce::Justification::centredLeft);
    g.setFont (valueFont (23.0f));
    g.drawText (e.bpmText, juce::Rectangle<float> (r.getX() + 110.0f, r.getY(), r.getWidth() - 122.0f, r.getHeight()),
                juce::Justification::centredRight);
    g.setColour (colours::creamDim);
    g.setFont (labelFont (16.0f));
    g.drawText (e.layoutText, kLayoutReadout.toFloat(), juce::Justification::centredRight);
}

bool LabdEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    proc.getEditHistory().service();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown())
            proc.getUndoManager().redo();
        else
            proc.getUndoManager().undo();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
    {
        proc.getUndoManager().redo();
        return true;
    }
    return false;
}

void LabdEditor::mouseDown (const juce::MouseEvent&)
{
    // De pulsar a soltar: todos los parámetros tocados forman una sola acción de undo.
    proc.getEditHistory().beginGroup();
}

void LabdEditor::mouseUp (const juce::MouseEvent&)
{
    proc.getEditHistory().endGroup();
}

} // namespace labd
