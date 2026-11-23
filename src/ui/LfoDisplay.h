// VitaPlug LAB-D · pantalla del LFO: forma de onda, fase actual y panorama
// efectivo del wet (datos reales del motor). Arrastrar sobre la línea L-R fija el
// PAN base del eco; doble clic lo centra.
#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/LabdLookAndFeel.h"

namespace labd::ui {

class LfoDisplay final : public juce::Component {
public:
    explicit LfoDisplay (LabdProcessor& p);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    juce::String rateText() const { return rateLabel; }

private:
    float panX (float pan) const noexcept;
    void setPanFromX (float x);

    LabdProcessor& proc;
    LfoShape shape = LfoShape::Sine;
    bool on = false, autoMode = false, mono = false, draggingPan = false;
    float phase = 0.0f, value = 0.0f, gate = 0.0f, autoScale = 1.0f, effPan = 0.0f, basePan = 0.0f, phaseOffset = 0.0f;
    juce::String rateLabel;
    juce::String lastKey;
};

} // namespace labd::ui
