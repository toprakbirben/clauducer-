#include "LogPanel.h"
#include "AppFont.h"

LogPanel::LogPanel()
{
    text.setMultiLine(true, true);
    text.setReadOnly(true);
    text.setCaretVisible(false);
    text.setScrollbarsShown(true);
    text.setFont(appFont(13.0f));
    text.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0x80eceff6));
    text.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffd7dbe8));
    text.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffd7dbe8));
    text.setColour(juce::TextEditor::textColourId, juce::Colour(0xff1c2030));
    addAndMakeVisible(text);
}

void LogPanel::addLine(const juce::String& line)
{
    auto stamp = juce::Time::getCurrentTime().formatted("%H:%M:%S");
    text.moveCaretToEnd();
    text.insertTextAtCaret((text.isEmpty() ? "" : "\n") + stamp + "  " + line);
}

void LogPanel::clear()
{
    text.clear();
}

void LogPanel::resized()
{
    text.setBounds(getLocalBounds());
}
