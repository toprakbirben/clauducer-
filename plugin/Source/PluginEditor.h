#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ResultsListComponent.h"
#include "CircularCaptureButton.h"
#include "RoundedLookAndFeel.h"
#include "LogPanel.h"
#include "LoginOverlay.h"
#include "PreviewPlayer.h"
#include "AudioDropZone.h"

/** Top-level plugin UI. Two views animated between with juce::ComponentAnimator:
      - Search view: prompt field + big circular capture button.
      - Focus view: entered automatically once results arrive -- the search
        controls slide off and fade out, and the results list expands to
        fill the whole editor. A back button returns to the search view.
    Derives from juce::DragAndDropContainer so ResultsListComponent's rows
    have somewhere to originate their external (OS-level) drag from -- this
    is the standard JUCE pattern for a plugin editor that needs drag-out.

    In the DAW, the capture button records the next 4 or 8 bars of track
    audio (see ClauducerAudioProcessor::startBarCapture) and searches with
    it. A file dropped on the tile beside the prompt field (or anywhere on
    the window, or picked by clicking the tile) replaces that capture; the
    Standalone build has no track audio, so there a file is required.
*/
class ClauducerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       public juce::FileDragAndDropTarget,
                                       private ClauducerAudioProcessor::SearchListener
{
public:
    explicit ClauducerAudioProcessorEditor(ClauducerAudioProcessor&);
    ~ClauducerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // juce::FileDragAndDropTarget (a dropped file replaces live capture)
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    // ClauducerAudioProcessor::SearchListener
    void searchStarted() override;
    void searchCompleted(const BackendClient::SearchResponse&) override;
    void searchFailed(const juce::String& errorMessage) override;
    void searchLog(const juce::String& line) override;
    void captureStatus(const juce::String& status) override;

    void onCaptureButtonClicked();
    void chooseReferenceFile();
    void setReferenceFile(const juce::File& file);
    void clearReferenceFile();
    void setStatus(const juce::String& text, bool isError);
    // A 401 from the backend means the Splice token is gone -- re-check and show the login overlay.
    void recheckAuthIfUnauthorized(const juce::String& errorMessage);

    // Lays out (or animates towards, if animate is true) either the search
    // view or the focus view, and toggles which components can receive
    // mouse input so the hidden/faded-out view doesn't steal clicks.
    void setFocusedView(bool shouldFocus, bool animate);
    void moveComponent(juce::Component& c, juce::Rectangle<int> targetBounds, float targetAlpha, bool animate);

    // Named audioProcessor (not processor) -- juce::AudioProcessorEditor
    // already has a protected `AudioProcessor& processor` member, and a
    // same-named field here would shadow it.
    ClauducerAudioProcessor& audioProcessor;
    bool focusedView = false;
    bool logPanelShown = false; // revealed once the first search starts

    // A user-picked reference file replaces live capture (always, in Standalone).
    const bool isStandalone;
    juce::File referenceFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Label promptLabel { {}, "What kind of sample do you want" };
    // Declared before promptEditor so it outlives it (members are destroyed
    // in reverse declaration order) -- the TextEditor keeps a raw pointer
    // to it via setLookAndFeel().
    RoundedTextEditorLookAndFeel promptEditorLookAndFeel;
    juce::TextEditor promptEditor;
    AudioDropZone audioDropZone;
    CircularCaptureButton captureButton;
    juce::TextButton fourBarsButton { "4 bars" }, eightBarsButton { "8 bars" }; // DAW only
    juce::Label statusLabel;
    LogPanel logPanel;
    juce::TextButton backButton { juce::CharPointer_UTF8("\xe2\x86\x90 Back") }; // "← Back"
    ResultsListComponent resultsList;
    PreviewPlayer previewPlayer; // hidden; plays result previews
    LoginOverlay loginOverlay; // last, so it sits above everything else

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClauducerAudioProcessorEditor)
};
