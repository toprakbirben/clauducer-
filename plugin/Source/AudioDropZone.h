#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Rounded tile that sits beside the prompt field: drop a reference audio
    file onto it, or click it to open a file chooser. Shows a waveform icon,
    and the loaded file's name once one is set -- plus a small clear (x)
    button in its top-right corner. */
class AudioDropZone : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    AudioDropZone() = default;

    std::function<void(const juce::File&)> onFileDropped;
    std::function<void()> onClick;
    std::function<void()> onClear;

    void setFileName(const juce::String& name);

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    juce::Rectangle<float> clearButtonArea() const;

    juce::String fileName;
    bool dragHovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDropZone)
};
