// VitaPlug LAB-D · aspecto del editor.
// Todo se dibuja en el espacio de diseño del frontal aprobado (2640 x 1200 px);
// el editor aplica una única escala, así que el texto y los vectores se
// rasterizan nítidos a la resolución real de la pantalla (Retina incluida).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace labd::ui {

constexpr int kDesignWidth = 2640;
constexpr int kDesignHeight = 1200;

namespace colours {
const juce::Colour cream { 0xffe8e2cc };
const juce::Colour creamDim { 0xff9fa290 };
const juce::Colour amber { 0xffeec389 };
const juce::Colour amberDim { 0xff8d816a };
const juce::Colour screen { 0xff0e1710 };
const juce::Colour screenEdge { 0xff2b3629 };
const juce::Colour grid { 0x3387a66b };
const juce::Colour curve { 0xffc5df91 };
const juce::Colour ledOn { 0xffc4e89a };
const juce::Colour ledOff { 0xff3c4136 };
const juce::Colour buttonOff { 0xff2a2d2b };
const juce::Colour buttonOn { 0xff3e5334 };
} // namespace colours

juce::Font labelFont (float height);   // sans en negrita con espaciado (rótulos y teclas)
juce::Font valueFont (float height);   // monoespaciada para lecturas

void drawKnob (juce::Graphics& g, juce::Point<float> centre, float radius, float proportion, bool enabled);
void drawFaderHandle (juce::Graphics& g, juce::Rectangle<float> r, bool active);
void drawDisplayBox (juce::Graphics& g, juce::Rectangle<float> r);
void drawLed (juce::Graphics& g, juce::Point<float> c, float radius, bool on);

class LabdLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LabdLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override { return valueFont (21.0f); }
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
};

// --- Widgets -------------------------------------------------------------------

// Mando giratorio. Doble clic recupera el valor por defecto, Mayúsculas ajusta
// fino y las flechas/AvPág/Inicio/Fin funcionan con el foco del teclado.
class Knob final : public juce::Slider {
public:
    Knob();
    void setOverride (bool active, double proportion = 1.0) { overrideActive = active; overrideProportion = proportion; }
    bool isOverridden() const noexcept { return overrideActive; }
    double overrideValue() const noexcept { return overrideProportion; }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }
    bool keyPressed (const juce::KeyPress&) override;
    void resetToDefault();
    double defaultValue = 0.0;

private:
    bool overrideActive = false;
    double overrideProportion = 1.0;
};

// Lectura de valor; doble clic abre la entrada de texto precisa.
class ValueDisplay final : public juce::Component {
public:
    enum class Style { Box, Large, Plain, PlainRight };
    explicit ValueDisplay (Style s = Style::Box) : style (s) {}
    void setText (const juce::String& main, const juce::String& sub = {});
    void setColour (juce::Colour c)
    {
        if (c != textColour)
        {
            textColour = c;
            repaint();
        }
    }
    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    std::function<void (const juce::String&)> onTextEntered; // nullptr = no editable
    std::function<juce::String()> textForEditing;
    juce::Rectangle<int> subArea; // zona de la línea secundaria (relativa al padre)

private:
    Style style;
    juce::String text, subText;
    juce::Colour textColour = colours::cream;
    std::unique_ptr<juce::TextEditor> editor;
};

// Tecla del frontal: interruptor con LED, segmento de un selector o tecla de banda.
class PanelButton final : public juce::Button {
public:
    enum class Kind { Led, Segment, Band };
    PanelButton (const juce::String& text, Kind k);
    void setLit (bool lit) { if (lit != isLit) { isLit = lit; repaint(); } }
    void setMarker (bool m) { if (m != marker) { marker = m; repaint(); } }
    void setDimmed (bool d) { if (d != dimmed) { dimmed = d; repaint(); } }
    void setLabel (const juce::String& t) { if (t != getButtonText()) { setButtonText (t); repaint(); } }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    // Altura de letra que cabe en 'width' (máx. base, mín. 15 px de diseño).
    static float fittedFontHeight (const juce::String& text, float width, float base);
    juce::Rectangle<float> textArea() const;
    float fontBase() const noexcept { return kind == Kind::Band ? 22.0f : 20.0f; }

private:
    Kind kind;
    bool isLit = false, marker = false, dimmed = false;
};

} // namespace labd::ui
