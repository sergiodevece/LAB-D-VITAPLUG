// VitaPlug LAB-D · historial de edición (undo/redo) del editor.
//
// Solo registra cambios hechos con gestos del plugin (beginChangeGesture /
// endChangeGesture: mandos, teclas, nodos, entrada de texto). La automatización
// del host no abre gestos y no entra en el historial. (Con un UndoManager
// conectado al AudioProcessorValueTreeState, cada cambio automatizado se apilaría
// en la transacción abierta y el historial crecería sin límite durante la
// reproducción.)
//
// Cada parámetro guarda su valor al empezar el gesto y al terminarlo; un grupo
// (begin/end, p. ej. de pulsar a soltar el ratón) reúne varios parámetros en una
// sola acción de undo.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace labd {

class EditHistory final : private juce::AudioProcessorParameter::Listener {
public:
    EditHistory (juce::AudioProcessor& processor, juce::UndoManager& undoManager);
    ~EditHistory() override;

    void beginGroup();
    void endGroup();
    void closeAllGroups(); // p. ej. al cerrar el editor a mitad de un arrastre

private:
    struct Change
    {
        juce::AudioProcessorParameter* param;
        float before, after;
    };

    class Action final : public juce::UndoableAction {
    public:
        Action (EditHistory& h, std::vector<Change> c) : history (h), changes (std::move (c)) {}
        bool perform() override;
        bool undo() override;
        int getSizeInUnits() override { return (int) changes.size() * 4 + 8; }

    private:
        void apply (bool after);
        EditHistory& history;
        std::vector<Change> changes;
        bool firstPerform = true; // la primera vez el cambio ya está hecho
    };

    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override;
    void commitIfIdle();

    juce::AudioProcessor& processor;
    juce::UndoManager& undo;
    std::map<int, Change> pending;
    int openGestures = 0, groupDepth = 0;
    bool applying = false;
};

} // namespace labd
