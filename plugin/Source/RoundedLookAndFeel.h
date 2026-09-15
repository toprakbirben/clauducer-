#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Draws a TextEditor's background/outline as a rounded rectangle instead of
    LookAndFeel_V4's default sharp corners -- used for the prompt field to
    match the rest of the UI's soft, rounded aesthetic. */
class RoundedTextEditorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, (float) width, (float) height, kCornerRadius);
    }

    void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& editor) override
    {
        if (! editor.isEnabled())
            return;

        const bool focused = editor.hasKeyboardFocus(true) && ! editor.isReadOnly();
        g.setColour(editor.findColour(focused ? juce::TextEditor::focusedOutlineColourId
                                               : juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f,
                                kCornerRadius, focused ? 2.0f : 1.0f);
    }

private:
    static constexpr float kCornerRadius = 12.0f;
};
