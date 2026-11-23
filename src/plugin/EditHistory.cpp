#include "plugin/EditHistory.h"

namespace labd {

EditHistory::EditHistory (juce::AudioProcessor& p, juce::UndoManager& um) : processor (p), undo (um)
{
    for (auto* param : processor.getParameters())
        param->addListener (this);
}

EditHistory::~EditHistory()
{
    for (auto* param : processor.getParameters())
        param->removeListener (this);
}

void EditHistory::parameterGestureChanged (int index, bool starting)
{
    // Los gestos del plugin nacen en el hilo de mensajes (editor). Las llamadas
    // desde otros hilos se ignoran para no tocar el historial sin bloqueo.
    if (applying || ! juce::MessageManager::existsAndIsCurrentThread())
        return;
    auto* param = processor.getParameters()[index];
    if (param == nullptr)
        return;
    if (starting)
    {
        ++openGestures;
        if (pending.find (index) == pending.end())
            pending[index] = { param, param->getValue(), param->getValue() };
    }
    else
    {
        openGestures = juce::jmax (0, openGestures - 1);
        auto it = pending.find (index);
        if (it != pending.end())
            it->second.after = param->getValue();
        commitIfIdle();
    }
}

void EditHistory::beginGroup()
{
    ++groupDepth;
}

void EditHistory::endGroup()
{
    groupDepth = juce::jmax (0, groupDepth - 1);
    commitIfIdle();
}

void EditHistory::closeAllGroups()
{
    groupDepth = 0;
    openGestures = 0;
    commitIfIdle();
}

void EditHistory::commitIfIdle()
{
    if (groupDepth > 0 || openGestures > 0 || pending.empty())
        return;
    std::vector<Change> changes;
    for (auto& [index, c] : pending)
    {
        juce::ignoreUnused (index);
        if (c.after != c.before)
            changes.push_back (c);
    }
    pending.clear();
    if (changes.empty())
        return;
    undo.beginNewTransaction();
    undo.perform (new Action (*this, std::move (changes)));
    undo.beginNewTransaction();
}

void EditHistory::Action::apply (bool after)
{
    const juce::ScopedValueSetter<bool> guard (history.applying, true);
    for (const auto& c : changes)
    {
        c.param->beginChangeGesture();
        c.param->setValueNotifyingHost (after ? c.after : c.before);
        c.param->endChangeGesture();
    }
}

bool EditHistory::Action::perform()
{
    if (firstPerform)
    {
        firstPerform = false;
        return true;
    }
    apply (true);
    return true;
}

bool EditHistory::Action::undo()
{
    apply (false);
    return true;
}

} // namespace labd
