#include "ui/LabdLookAndFeel.h"

namespace labd::ui {

namespace {
constexpr float kPi = juce::MathConstants<float>::pi;
constexpr float kStartAngle = -0.75f * kPi; // -135°
constexpr float kEndAngle = 0.75f * kPi;    // +135°
} // namespace

juce::Font labelFont (float height)
{
    return juce::Font (juce::FontOptions ("Verdana", height, juce::Font::bold)).withExtraKerningFactor (0.10f);
}

juce::Font valueFont (float height)
{
    return juce::Font (juce::FontOptions ("Menlo", height, juce::Font::plain));
}

void drawKnob (juce::Graphics& g, juce::Point<float> c, float r, float proportion, bool enabled)
{
    juce::Graphics::ScopedSaveState save (g);
    // setOpacity() solo afecta al color actual: los estados atenuados usan una capa.
    if (! enabled)
        g.beginTransparencyLayer (0.45f);

    // Sombra
    {
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.55f), c.x, c.y + r * 0.10f,
                                     juce::Colours::transparentBlack, c.x, c.y + r * 1.18f, true);
        g.setGradientFill (shadow);
        g.fillEllipse (c.x - r * 1.18f, c.y - r * 1.08f, r * 2.36f, r * 2.36f);
    }

    // Aro moleteado (tonos medidos en el frontal: base ~#2a2a24, dientes ~#424239)
    g.setColour (juce::Colour (0xff23231e));
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    const int teeth = 80;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = (float) i / teeth * 2.0f * kPi;
        const juce::Point<float> d (std::sin (a), -std::cos (a));
        g.setColour (i % 2 ? juce::Colour (0xff47473d) : juce::Colour (0xff1b1b17));
        g.drawLine ({ c + d * (r * 0.86f), c + d * (r * 0.975f) }, r * 0.034f);
    }
    g.setColour (juce::Colour (0xff121210));
    g.drawEllipse (c.x - r * 0.985f, c.y - r * 0.985f, r * 1.97f, r * 1.97f, r * 0.025f);

    // Tapa metálica cepillada: degradado cónico de cuatro lóbulos, beige cálido
    // (medido en el frontal: oscuro ~#5d5444, claro ~#cbbfa2).
    const float rc = r * 0.86f;
    const int sectors = 120;
    const juce::Colour dark (0xff57503f), light (0xffd6cbad);
    for (int i = 0; i < sectors; ++i)
    {
        const float a0 = (float) i / sectors * 2.0f * kPi;
        const float a1 = (float) (i + 1) / sectors * 2.0f * kPi + 0.012f;
        const float mid = 0.5f * (a0 + a1);
        const float b = 0.5f + 0.42f * std::cos (4.0f * mid + 0.15f) + 0.12f * std::cos (2.0f * mid - 0.9f)
                        + 0.03f * std::sin (mid * 23.0f);
        juce::Path wedge;
        wedge.addPieSegment (c.x - rc, c.y - rc, 2 * rc, 2 * rc, a0, a1, 0.0f);
        g.setColour (dark.interpolatedWith (light, juce::jlimit (0.0f, 1.0f, b)));
        g.fillPath (wedge);
    }
    {
        juce::ColourGradient centre (juce::Colours::white.withAlpha (0.10f), c.x, c.y, juce::Colours::transparentWhite,
                                     c.x + rc * 0.75f, c.y, true);
        g.setGradientFill (centre);
        g.fillEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc);
        juce::ColourGradient shade (juce::Colours::transparentBlack, c.x, c.y - rc,
                                    juce::Colours::black.withAlpha (0.16f), c.x, c.y + rc, false);
        g.setGradientFill (shade);
        g.fillEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc);
    }
    g.setColour (juce::Colour (0xff2c2a22));
    g.drawEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc, r * 0.022f);

    // Índice: línea crema con sombra a un lado (como el frontal).
    const float a = kStartAngle + juce::jlimit (0.0f, 1.0f, proportion) * (kEndAngle - kStartAngle);
    const juce::Point<float> d (std::sin (a), -std::cos (a));
    const juce::Line<float> pointer (c + d * (rc * 0.60f), c + d * (rc * 0.94f));
    const float w = juce::jmax (2.5f, r * 0.065f);
    g.setColour (juce::Colour (0x99201d16));
    g.drawLine (pointer.getStart().x + w * 0.45f, pointer.getStart().y + w * 0.45f,
                pointer.getEnd().x + w * 0.45f, pointer.getEnd().y + w * 0.45f, w);
    g.setColour (juce::Colour (0xfff4ecd4));
    g.drawLine (pointer, w);
    if (! enabled)
        g.endTransparencyLayer();
}

void drawFaderHandle (juce::Graphics& g, juce::Rectangle<float> r, bool active)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded (1.0f), 3.0f);
    const juce::Colour top = active ? juce::Colour (0xffd3ad80) : juce::Colour (0xffcfc6b2);
    const juce::Colour bottom = active ? juce::Colour (0xff8b6a47) : juce::Colour (0xff6f6858);
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colour (0xff2a261f));
    g.drawRoundedRectangle (r, 3.0f, 1.2f);
    g.setColour (juce::Colour (0xfff1e8d2).withAlpha (0.9f));
    g.fillRect (r.withSizeKeepingCentre (r.getWidth() * 0.62f, 1.6f));
}

void drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c130e), r.getX(), r.getY(), juce::Colour (0xff111b13),
                                             r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0xff27302a));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
}

void drawLed (juce::Graphics& g, juce::Point<float> c, float radius, bool on)
{
    if (on)
    {
        juce::ColourGradient glow (colours::ledOn.withAlpha (0.55f), c.x, c.y, colours::ledOn.withAlpha (0.0f), c.x + radius * 2.6f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - radius * 2.6f, c.y - radius * 2.6f, radius * 5.2f, radius * 5.2f);
    }
    g.setColour (on ? colours::ledOn : colours::ledOff);
    g.fillEllipse (c.x - radius, c.y - radius, 2 * radius, 2 * radius);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - radius, c.y - radius, 2 * radius, 2 * radius, 1.0f);
}

LabdLookAndFeel::LabdLookAndFeel()
{
    setColour (juce::TextEditor::backgroundColourId, colours::screen);
    setColour (juce::TextEditor::textColourId, colours::amber);
    setColour (juce::TextEditor::highlightColourId, colours::buttonOn);
    setColour (juce::TextEditor::outlineColourId, colours::curve);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::curve);
    setColour (juce::CaretComponent::caretColourId, colours::amber);
}

void LabdLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider& s)
{
    auto* knob = dynamic_cast<Knob*> (&s);
    const float p = (knob != nullptr && knob->isOverridden()) ? (float) knob->overrideValue() : pos;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    drawKnob (g, bounds.getCentre(), juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f, p, s.isEnabled());
}

void LabdLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                        juce::Slider::SliderStyle style, juce::Slider& s)
{
    const bool active = s.getValue() > s.getMinimum() + 1.0e-6;
    juce::Graphics::ScopedSaveState save (g);
    const bool dim = ! s.isEnabled();
    if (dim)
        g.beginTransparencyLayer (0.4f);
    if (style == juce::Slider::LinearVertical)
    {
        const float cx = (float) x + (float) w * 0.5f;
        const auto slot = juce::Rectangle<float> (cx - 4.0f, (float) y - 2.0f, 8.0f, (float) h + 4.0f);
        g.setColour (juce::Colour (0xff0f130e));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (slot.expanded (0.5f), 3.5f, 1.0f);
        drawFaderHandle (g, juce::Rectangle<float> (26.0f, 16.0f).withCentre ({ cx, pos }), active);
    }
    else
    {
        const float cy = (float) y + (float) h * 0.5f;
        const auto slot = juce::Rectangle<float> ((float) x - 9.0f, cy - 4.0f, (float) w + 18.0f, 8.0f);
        g.setColour (juce::Colour (0xff0f130e));
        g.fillRoundedRectangle (slot, 3.5f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawRoundedRectangle (slot.expanded (0.5f), 3.5f, 1.0f);
        if (active)
        {
            g.setColour (colours::amber.withAlpha (0.35f));
            g.fillRoundedRectangle (slot.withRight (pos).reduced (1.5f), 2.5f);
        }
        drawFaderHandle (g, juce::Rectangle<float> (20.0f, 18.0f).withCentre ({ pos, cy }), active);
    }
    if (dim)
        g.endTransparencyLayer();
}

juce::Slider::SliderLayout LabdLookAndFeel::getSliderLayout (juce::Slider& s)
{
    juce::Slider::SliderLayout layout;
    auto b = s.getLocalBounds();
    if (s.getSliderStyle() == juce::Slider::LinearVertical)
        b = b.reduced (0, 8);   // centro del tirador: de 0 a 100 %
    else if (s.getSliderStyle() == juce::Slider::LinearHorizontal)
        b = b.reduced (10, 0);
    layout.sliderBounds = b;
    return layout;
}

// --- Knob ------------------------------------------------------------------------

Knob::Knob() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
{
    setRotaryParameters (kStartAngle + 2.0f * kPi, kEndAngle + 2.0f * kPi, true);
    setMouseDragSensitivity (300);
    setWantsKeyboardFocus (true);
    setScrollWheelEnabled (true);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1200 : 300); // Mayúsculas: ajuste fino
    if (onGestureStart)
        onGestureStart();
    juce::Slider::mouseDown (e);
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled())
        return;
    if (onGestureStart)
        onGestureStart();
    setValue (defaultValue, juce::sendNotificationSync);
}

bool Knob::keyPressed (const juce::KeyPress& key)
{
    if (! isEnabled())
        return false;
    const auto code = key.getKeyCode();
    const bool fine = key.getModifiers().isShiftDown();
    double step = 0.0;
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey)
        step = fine ? 0.002 : 0.01;
    else if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey)
        step = fine ? -0.002 : -0.01;
    else if (code == juce::KeyPress::pageUpKey)
        step = 0.05;
    else if (code == juce::KeyPress::pageDownKey)
        step = -0.05;
    else if (code == juce::KeyPress::homeKey || code == juce::KeyPress::endKey)
    {
        if (onGestureStart)
            onGestureStart();
        setValue (code == juce::KeyPress::homeKey ? getMinimum() : getMaximum(), juce::sendNotificationSync);
        return true;
    }
    else
        return false;

    if (onGestureStart)
        onGestureStart();
    const double interval = getInterval();
    double target = proportionOfLengthToValue (juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + step));
    if (interval > 0.0 && std::abs (target - getValue()) < interval)
        target = getValue() + (step > 0 ? interval : -interval); // selectores: al menos un paso
    setValue (juce::jlimit (getMinimum(), getMaximum(), target), juce::sendNotificationSync);
    return true;
}

// --- ValueDisplay --------------------------------------------------------------------

void ValueDisplay::setText (const juce::String& main, const juce::String& sub)
{
    if (main == text && sub == subText)
        return;
    text = main;
    subText = sub;
    repaint();
}

void ValueDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (style == Style::Large)
    {
        auto box = r.removeFromTop (55.0f);
        drawDisplayBox (g, box);
        g.setColour (isEnabled() ? colours::amber : colours::amberDim);
        g.setFont (valueFont (36.0f));
        g.drawText (text, box, juce::Justification::centred, false);
        g.setColour (colours::creamDim.brighter (0.25f));
        g.setFont (valueFont (18.0f));
        g.drawText (subText, r, juce::Justification::centred, false);
    }
    else if (style == Style::Box)
    {
        drawDisplayBox (g, r);
        g.setColour (isEnabled() ? textColour : textColour.withAlpha (0.45f));
        g.setFont (valueFont (22.0f));
        g.drawText (text, r, juce::Justification::centred, false);
    }
    else
    {
        g.setColour (textColour);
        g.setFont (valueFont (19.0f));
        g.drawText (text, r, style == Style::PlainRight ? juce::Justification::centredRight : juce::Justification::centred, false);
    }
}

void ValueDisplay::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onTextEntered == nullptr || ! isEnabled())
        return;
    editor = std::make_unique<juce::TextEditor>();
    addAndMakeVisible (*editor);
    const auto box = style == Style::Large ? getLocalBounds().removeFromTop (55) : getLocalBounds();
    editor->setBounds (box);
    editor->setFont (valueFont (style == Style::Large ? 30.0f : 20.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (textForEditing ? textForEditing() : text, false);
    editor->selectAll();
    auto finish = [this] (bool commit) {
        if (editor == nullptr)
            return;
        const auto t = editor->getText();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ValueDisplay> (this)] {
            if (safe != nullptr)
                safe->editor.reset();
        });
        if (commit && onTextEntered)
            onTextEntered (t);
    };
    editor->onReturnKey = [finish] { finish (true); };
    editor->onEscapeKey = [finish] { finish (false); };
    editor->onFocusLost = [finish] { finish (false); };
    editor->grabKeyboardFocus();
}

// --- PanelButton ------------------------------------------------------------------------

PanelButton::PanelButton (const juce::String& t, Kind k) : juce::Button (t), kind (k)
{
    setWantsKeyboardFocus (false);
}

float PanelButton::fittedFontHeight (const juce::String& text, float width, float base)
{
    float h = base;
    while (h > 15.0f && juce::GlyphArrangement::getStringWidth (labelFont (h), text) > width)
        h -= 0.5f;
    return h;
}

juce::Rectangle<float> PanelButton::textArea() const
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    if (kind == Kind::Led)
    {
        const float ledX = r.getX() + juce::jmin (24.0f, r.getHeight() * 0.45f);
        r.removeFromLeft (ledX + 8.0f - r.getX());
        r.removeFromRight (6.0f);
    }
    return r.reduced (7.0f, 0.0f);
}

void PanelButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    juce::Graphics::ScopedSaveState save (g);
    const bool dim = dimmed || ! isEnabled();
    if (dim)
        g.beginTransparencyLayer (0.45f);
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.5f), 6.0f);

    const bool green = isLit;
    juce::Colour top = green ? juce::Colour (0xff4a6340) : juce::Colour (0xff303331);
    juce::Colour bottom = green ? juce::Colour (0xff334629) : juce::Colour (0xff222523);
    if (highlighted && isEnabled())
    {
        top = top.brighter (0.08f);
        bottom = bottom.brighter (0.08f);
    }
    if (down)
        std::swap (top, bottom);
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (green ? juce::Colour (0xff98b07c) : juce::Colour (0xff101210));
    g.drawRoundedRectangle (r, 6.0f, green ? 1.6f : 1.4f);
    g.setColour (juce::Colours::white.withAlpha (green ? 0.10f : 0.07f));
    g.drawLine (r.getX() + 6.0f, r.getY() + 1.5f, r.getRight() - 6.0f, r.getY() + 1.5f, 1.0f);

    if (kind == Kind::Led)
        drawLed (g, { r.getX() + juce::jmin (24.0f, r.getHeight() * 0.45f), r.getCentreY() }, 5.5f, isLit);
    const auto area = textArea();
    g.setColour (colours::cream);
    const float fontH = fittedFontHeight (getButtonText(), area.getWidth(), fontBase());
    g.setFont (labelFont (fontH));
    g.drawText (getButtonText(), area, juce::Justification::centred, false);
    if (marker)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (labelFont (fontH), getButtonText());
        const juce::Point<float> dot (area.getCentreX() + tw * 0.5f + 7.0f, area.getCentreY() - 9.0f);
        g.setColour (colours::amber);
        g.fillEllipse (dot.x - 3.5f, dot.y - 3.5f, 7.0f, 7.0f);
    }
    if (dim)
        g.endTransparencyLayer();
}

} // namespace labd::ui
