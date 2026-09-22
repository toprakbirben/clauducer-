#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Read-only, timestamped step-by-step log of a search (analysis, feeling,
    query, results/errors), shown under the status label in the search view. */
class LogPanel : public juce::Component
{
public:
    LogPanel();

    void addLine(const juce::String& line);
    void clear();

    void resized() override;

private:
    juce::TextEditor text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogPanel)
};
