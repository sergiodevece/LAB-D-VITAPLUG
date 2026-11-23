// VitaPlug LAB-D · pantalla ECHO EQ.
// Curva base (verde) calculada con las mismas funciones de diseño y respuesta
// que procesan el audio; curva efectiva modulada (ámbar) a partir de las
// frecuencias reales que publica el motor.
#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/LabdLookAndFeel.h"

namespace labd::ui {

class EqDisplay final : public juce::Component {
public:
    explicit EqDisplay (LabdProcessor& p);

    void refresh();                 // desde el temporizador del editor
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void (int)> onBandSelected;

    // Geometría (coordenadas locales) expuesta para las pruebas del editor.
    juce::Rectangle<float> plotArea() const noexcept;
    float xForFreq (double hz) const noexcept;
    double freqForX (float x) const noexcept;
    float yForDb (double db) const noexcept;
    double dbForY (float y) const noexcept;
    juce::Point<float> nodePosition (int band) const;
    double responseDbAt (double hz, bool modulated) const;
    bool isModulating() const noexcept { return modulating; }

private:
    struct BandView
    {
        bool on = true;
        double freq = 1000.0, gain = 0.0, q = 0.707, effectiveFreq = 1000.0;
        FilterType type = FilterType::Bell;
        float depth = 0.0f;
    };

    void setParam (const juce::String& id, float value);
    juce::Path buildCurve (bool modulated, bool closeForFill) const;

    LabdProcessor& proc;
    std::array<BandView, kNumEqBands> bands {};
    bool eqOn = true, bypassed = false, modulating = false;
    int selected = 2;
    int dragging = -1;
    double sampleRate = 48000.0;
};

} // namespace labd::ui
