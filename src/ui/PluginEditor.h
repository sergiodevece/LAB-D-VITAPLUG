// VitaPlug LAB-D · editor.
// El frontal aprobado (LAB-D_Frontal.png) es el fondo fijo; mandos, teclas,
// curvas, lecturas y luces se dibujan encima y se controlan dinámicamente.
#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/EqDisplay.h"
#include "ui/LabdLookAndFeel.h"
#include "ui/LfoDisplay.h"

namespace labd {

class LabdEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit LabdEditor (LabdProcessor&);
    ~LabdEditor() override;

    static constexpr int kNominalWidth = 1320, kNominalHeight = 600;
    static constexpr int kMinWidth = 1100, kMinHeight = 500;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    // Para pruebas y capturas: refresco inmediato y acceso a componentes.
    void refreshNow() { timerCallback(); }
    juce::Component& content() noexcept { return panel; }
    juce::Component* findById (const juce::String& id) const;

private:
    class Panel final : public juce::Component {
    public:
        explicit Panel (LabdEditor& e) : owner (e) { setInterceptsMouseClicks (false, true); }
        void paint (juce::Graphics&) override;
        LabdEditor& owner;
    };

    void timerCallback() override;
    void restoreEditorSize();
    void buildControls();
    void attachTimeKnobs (bool sync, bool link);
    void attachEqKnobs (int band, bool shelf6);
    void attachRateKnob (bool sync);
    void selectBand (int band);
    void setChoice (const juce::String& id, int index);
    void setBool (const juce::String& id, bool value);
    float raw (const juce::String& id) const;
    void bindTextEntry (ui::ValueDisplay& display, std::function<juce::String()> paramId);
    void paintPatches (juce::Graphics& g);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    LabdProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    ui::LabdLookAndFeel lnf;
    juce::Image background;
    Panel panel { *this };
    bool restoringEditorSize = true; // los límites iniciales ya pueden llamar a resized()

    // Izquierda
    ui::Knob timeL, timeR, feedbackKnob, mixKnob, outputKnob;
    ui::ValueDisplay timeLDisplay { ui::ValueDisplay::Style::Large }, timeRDisplay { ui::ValueDisplay::Style::Large };
    ui::ValueDisplay feedbackDisplay, mixDisplay, outputDisplay;
    ui::PanelButton syncButton { "SYNC", ui::PanelButton::Kind::Led }, linkButton { "LINK", ui::PanelButton::Kind::Led };
    ui::PanelButton stereoButton { "STEREO", ui::PanelButton::Kind::Segment }, pingPongButton { "PING PONG", ui::PanelButton::Kind::Segment };
    ui::PanelButton cleanButton { "CLEAN", ui::PanelButton::Kind::Segment }, tapeButton { "TAPE", ui::PanelButton::Kind::Segment };
    juce::Slider driveSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider duckSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    ui::ValueDisplay driveValue { ui::ValueDisplay::Style::PlainRight }, duckValue { ui::ValueDisplay::Style::PlainRight };

    // Centro
    ui::EqDisplay eqDisplay { proc };
    ui::PanelButton eqInButton { "EQ IN", ui::PanelButton::Kind::Led };
    std::array<std::unique_ptr<ui::PanelButton>, kNumEqBands> bandKeys;
    ui::PanelButton typeButton { "BELL", ui::PanelButton::Kind::Segment };
    ui::PanelButton bandInButton { "IN", ui::PanelButton::Kind::Led };
    ui::Knob freqKnob, gainKnob, qKnob;
    ui::ValueDisplay freqDisplay, gainDisplay, qDisplay;

    // Derecha
    ui::LfoDisplay lfoDisplay { proc };
    ui::PanelButton autoButton { "AUTO", ui::PanelButton::Kind::Segment }, manualButton { "MANUAL", ui::PanelButton::Kind::Segment };
    ui::PanelButton lfoOnButton { "ON", ui::PanelButton::Kind::Led };
    std::array<std::unique_ptr<ui::PanelButton>, 5> shapeButtons;
    ui::Knob rateKnob, phaseKnob;
    ui::ValueDisplay rateDisplay, phaseDisplay;
    ui::PanelButton rateSyncButton { "SYNC", ui::PanelButton::Kind::Led };
    std::array<std::unique_ptr<juce::Slider>, kNumLfoDepths> depthFaders;
    std::array<std::unique_ptr<ui::ValueDisplay>, kNumLfoDepths> depthValues;

    // Pie
    ui::PanelButton bypassButton { "BYPASS", ui::PanelButton::Kind::Led }, wetOnlyButton { "WET ONLY", ui::PanelButton::Kind::Led };

    // Enlaces con parámetros
    std::unique_ptr<SliderAttachment> timeLAtt, timeRAtt, feedbackAtt, mixAtt, outputAtt, driveAtt, duckAtt;
    std::unique_ptr<SliderAttachment> freqAtt, gainAtt, qAtt, rateAtt, phaseAtt;
    std::array<std::unique_ptr<SliderAttachment>, kNumLfoDepths> depthAtts;
    std::unique_ptr<ButtonAttachment> syncAtt, linkAtt, eqInAtt, bandInAtt, lfoOnAtt, rateSyncAtt, bypassAtt, wetOnlyAtt;

    juce::String timeLParam, timeRParam, freqParam, gainParam, qParam, rateParam;
    int attachedBand = -1, attachedShelf = -1, attachedTimeMode = -1, attachedRateSync = -1;

    // Pie: lectura del BPM y disposición de canales
    juce::String bpmText, layoutText;
    bool bpmFromHost = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabdEditor)
};

} // namespace labd
