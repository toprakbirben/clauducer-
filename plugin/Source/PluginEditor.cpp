#include "PluginEditor.h"
#include "BackgroundGradient.h"
#include "AppFont.h"

namespace
{
    const juce::Colour kFieldBackground { 0xffeceff6 };
    const juce::Colour kFieldBorder { 0xffd7dbe8 };
    const juce::Colour kTextPrimary { 0xff1c2030 };
    constexpr int kAnimationMs = 320;
    const juce::String kAudioFilePatterns = "*.wav;*.aif;*.aiff;*.mp3;*.flac";

    bool isSupportedAudioFile(const juce::File& f)
    {
        return f.existsAsFile() && f.hasFileExtension("wav;aif;aiff;mp3;flac");
    }
}

ClauducerAudioProcessorEditor::ClauducerAudioProcessorEditor(ClauducerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      isStandalone(p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      resultsList(p)
{
    audioProcessor.addSearchListener(this);

    promptLabel.setFont(appFont(22.0f));
    promptLabel.setColour(juce::Label::textColourId, kTextPrimary);
    addAndMakeVisible(promptLabel);

    promptEditor.setMultiLine(false);
    promptEditor.setLookAndFeel(&promptEditorLookAndFeel);
    promptEditor.setFont(appFont(17.0f));
    promptEditor.setColour(juce::TextEditor::backgroundColourId, kFieldBackground);
    promptEditor.setColour(juce::TextEditor::outlineColourId, kFieldBorder);
    promptEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff6f9dff));
    promptEditor.setColour(juce::TextEditor::textColourId, kTextPrimary);
    promptEditor.onTextChange = [this] { captureButton.setLocked(false); };
    addAndMakeVisible(promptEditor);

    captureButton.onClick = [this] { onCaptureButtonClicked(); };
    addAndMakeVisible(captureButton);

    statusLabel.setFont(appFont(14.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    addAndMakeVisible(logPanel);

    backButton.onClick = [this] { setFocusedView(false, true); };
    addAndMakeVisible(backButton);

    resultsList.onDragError = [this](const juce::String& message)
    {
        setStatus(message, true);
        recheckAuthIfUnauthorized(message);
    };
    addAndMakeVisible(resultsList);

    loginOverlay.onVisibilityChanged = [this](bool isShowing) { captureButton.setVisible(!isShowing); };
    addChildComponent(loginOverlay);

    setSize(520, 680);
    setFocusedView(false, false);

    if (isStandalone)
        setStatus("Drop an audio file here, or click the button to choose one.", false);

    loginOverlay.check();
}

ClauducerAudioProcessorEditor::~ClauducerAudioProcessorEditor()
{
    audioProcessor.removeSearchListener(this);
}

void ClauducerAudioProcessorEditor::paint(juce::Graphics& g)
{
    paintAIGradientBackground(g, getLocalBounds().toFloat(), getLocalBounds().toFloat(), {});
}

void ClauducerAudioProcessorEditor::resized()
{
    setFocusedView(focusedView, false);
    loginOverlay.setBounds(getLocalBounds());
}

void ClauducerAudioProcessorEditor::moveComponent(juce::Component& c, juce::Rectangle<int> targetBounds, float targetAlpha, bool animate)
{
    if (animate)
    {
        juce::Desktop::getInstance().getAnimator().animateComponent(&c, targetBounds, targetAlpha, kAnimationMs, false, 0.0, 0.0);
    }
    else
    {
        c.setBounds(targetBounds);
        c.setAlpha(targetAlpha);
    }
}

void ClauducerAudioProcessorEditor::setFocusedView(bool shouldFocus, bool animate)
{
    focusedView = shouldFocus;

    // More breathing room from the top/left/right edges than the bottom --
    // matches the gap the reference mock left before the "Kind" label.
    constexpr int kMarginSide = 28;
    constexpr int kMarginTop = 28;
    constexpr int kMarginBottom = 12;
    auto bounds = getLocalBounds().withTrimmedLeft(kMarginSide).withTrimmedRight(kMarginSide)
                                   .withTrimmedTop(kMarginTop).withTrimmedBottom(kMarginBottom);

    // Search-view layout (same geometry regardless of which view is active --
    // used both to place these components when visible and, offset above the
    // top edge, as where they animate to/from when hidden).
    auto area = bounds;
    auto promptLabelBounds = area.removeFromTop(26);
    area.removeFromTop(4);
    auto promptEditorBounds = area.removeFromTop(52);
    area.removeFromTop(12);
    constexpr int buttonSize = 200;
    auto captureButtonBounds = area.removeFromTop(buttonSize + 8).withSizeKeepingCentre(buttonSize, buttonSize);
    area.removeFromTop(4);
    auto statusLabelBounds = area.removeFromTop(20);
    area.removeFromTop(6);
    auto logPanelBounds = area.removeFromTop(120);
    area.removeFromTop(6);
    auto resultsNormalBounds = area;

    // Focus-view layout: back button top-left, results fill nearly everything.
    auto focusArea = bounds;
    auto backButtonBounds = focusArea.removeFromTop(28).removeFromLeft(90);
    focusArea.removeFromTop(8);
    auto resultsFocusedBounds = focusArea;

    const auto offscreenAbove = [this](juce::Rectangle<int> r) { return r.withY(r.getY() - getHeight()); };

    if (shouldFocus)
    {
        moveComponent(promptLabel, offscreenAbove(promptLabelBounds), 0.0f, animate);
        moveComponent(promptEditor, offscreenAbove(promptEditorBounds), 0.0f, animate);
        moveComponent(captureButton, offscreenAbove(captureButtonBounds), 0.0f, animate);
        moveComponent(statusLabel, offscreenAbove(statusLabelBounds), 0.0f, animate);
        moveComponent(logPanel, offscreenAbove(logPanelBounds), 0.0f, animate);
        moveComponent(backButton, backButtonBounds, 1.0f, animate);
        moveComponent(resultsList, resultsFocusedBounds, 1.0f, animate);
    }
    else
    {
        moveComponent(promptLabel, promptLabelBounds, 1.0f, animate);
        moveComponent(promptEditor, promptEditorBounds, 1.0f, animate);
        moveComponent(captureButton, captureButtonBounds, 1.0f, animate);
        moveComponent(statusLabel, statusLabelBounds, 1.0f, animate);
        moveComponent(logPanel, logPanelBounds, 1.0f, animate);
        moveComponent(backButton, offscreenAbove(backButtonBounds), 0.0f, animate);
        moveComponent(resultsList, resultsNormalBounds, 1.0f, animate);
    }

    // The faded-out view must not intercept clicks meant for the visible one.
    promptEditor.setInterceptsMouseClicks(!shouldFocus, !shouldFocus);
    captureButton.setInterceptsMouseClicks(!shouldFocus, !shouldFocus);
    logPanel.setInterceptsMouseClicks(!shouldFocus, !shouldFocus);
    backButton.setInterceptsMouseClicks(shouldFocus, shouldFocus);
}

void ClauducerAudioProcessorEditor::onCaptureButtonClicked()
{
    if (isStandalone)
    {
        if (!referenceFile.existsAsFile())
        {
            chooseReferenceFile();
            return;
        }
        if (promptEditor.getText().trim().isEmpty())
        {
            setStatus("Enter a prompt, then click again to search.", true);
            return;
        }
        audioProcessor.runSearch(referenceFile.getFullPathName(), promptEditor.getText());
        return;
    }

    auto capturedPath = audioProcessor.captureReferenceToTempFile(4.0);
    if (capturedPath.isEmpty())
    {
        setStatus("Nothing captured yet -- play some audio through this track first.", true);
        return;
    }

    audioProcessor.runSearch(capturedPath, promptEditor.getText());
}

void ClauducerAudioProcessorEditor::chooseReferenceFile()
{
    fileChooser = std::make_unique<juce::FileChooser>("Choose a reference audio file", juce::File(), kAudioFilePatterns);
    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this](const juce::FileChooser& chooser)
                             {
                                 auto file = chooser.getResult();
                                 if (file == juce::File())
                                     return; // cancelled
                                 setReferenceFile(file);
                                 if (referenceFile.existsAsFile() && promptEditor.getText().trim().isNotEmpty())
                                     audioProcessor.runSearch(referenceFile.getFullPathName(), promptEditor.getText());
                             });
}

void ClauducerAudioProcessorEditor::setReferenceFile(const juce::File& file)
{
    if (!isSupportedAudioFile(file))
    {
        setStatus("Unsupported file: " + file.getFileName() + " (use wav, aif, mp3 or flac)", true);
        return;
    }
    referenceFile = file;
    captureButton.setLocked(false);
    setStatus(file.getFileName() + (promptEditor.getText().trim().isEmpty() ? " -- now enter a prompt" : " -- click to search"), false);
}

bool ClauducerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return isStandalone && files.size() == 1 && isSupportedAudioFile(juce::File(files[0]));
}

void ClauducerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    if (focusedView)
        setFocusedView(false, true);
    setReferenceFile(juce::File(files[0]));
}

void ClauducerAudioProcessorEditor::setStatus(const juce::String& text, bool isError)
{
    statusLabel.setText(text, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, isError ? juce::Colours::orangered : juce::Colour(0xff1f9d63));
}

void ClauducerAudioProcessorEditor::searchStarted()
{
    captureButton.setAnimating(true);
    logPanel.clear();
    setStatus("Analyzing and searching Splice...", false);
}

void ClauducerAudioProcessorEditor::searchLog(const juce::String& line)
{
    logPanel.addLine(line);
}

void ClauducerAudioProcessorEditor::searchCompleted(const BackendClient::SearchResponse& response)
{
    captureButton.setAnimating(false);
    setStatus(juce::String(response.results.size()) + " results", false);
    resultsList.setResults(response.results);
    setFocusedView(true, true);
}

void ClauducerAudioProcessorEditor::searchFailed(const juce::String& errorMessage)
{
    captureButton.setAnimating(false);
    setStatus(errorMessage, true);
    recheckAuthIfUnauthorized(errorMessage);
}

void ClauducerAudioProcessorEditor::recheckAuthIfUnauthorized(const juce::String& errorMessage)
{
    if (errorMessage.contains("Backend error (401)"))
        loginOverlay.check();
}
