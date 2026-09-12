#include "PluginEditor.h"

namespace
{
    const juce::Colour kBackground { 0xff0b0d14 };
    const juce::Colour kFieldBackground { 0xff1b2030 };
    const juce::Colour kFieldBorder { 0xff333b52 };
    constexpr int kAnimationMs = 320;
}

ClauducerAudioProcessorEditor::ClauducerAudioProcessorEditor(ClauducerAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), resultsList(p)
{
    audioProcessor.addSearchListener(this);

    promptLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    promptLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(promptLabel);

    promptEditor.setMultiLine(false);
    promptEditor.setTextToShowWhenEmpty("e.g. a warm layering pad to sit under this vocal", juce::Colours::grey);
    promptEditor.setColour(juce::TextEditor::backgroundColourId, kFieldBackground);
    promptEditor.setColour(juce::TextEditor::outlineColourId, kFieldBorder);
    promptEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff6f9dff));
    promptEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(promptEditor);

    captureButton.onClick = [this] { onCaptureButtonClicked(); };
    addAndMakeVisible(captureButton);

    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    backButton.onClick = [this] { setFocusedView(false, true); };
    addAndMakeVisible(backButton);

    resultsList.onDragError = [this](const juce::String& message) { setStatus(message, true); };
    addAndMakeVisible(resultsList);

    setSize(520, 560);
    setFocusedView(false, false);
}

ClauducerAudioProcessorEditor::~ClauducerAudioProcessorEditor()
{
    audioProcessor.removeSearchListener(this);
}

void ClauducerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);
}

void ClauducerAudioProcessorEditor::resized()
{
    setFocusedView(focusedView, false);
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

    auto bounds = getLocalBounds().reduced(12);

    // Search-view layout (same geometry regardless of which view is active --
    // used both to place these components when visible and, offset above the
    // top edge, as where they animate to/from when hidden).
    auto area = bounds;
    auto promptLabelBounds = area.removeFromTop(20);
    area.removeFromTop(4);
    auto promptEditorBounds = area.removeFromTop(30);
    area.removeFromTop(12);
    constexpr int buttonSize = 160;
    auto captureButtonBounds = area.removeFromTop(buttonSize + 8).withSizeKeepingCentre(buttonSize, buttonSize);
    area.removeFromTop(4);
    auto statusLabelBounds = area.removeFromTop(20);
    area.removeFromTop(4);
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
        moveComponent(backButton, backButtonBounds, 1.0f, animate);
        moveComponent(resultsList, resultsFocusedBounds, 1.0f, animate);
    }
    else
    {
        moveComponent(promptLabel, promptLabelBounds, 1.0f, animate);
        moveComponent(promptEditor, promptEditorBounds, 1.0f, animate);
        moveComponent(captureButton, captureButtonBounds, 1.0f, animate);
        moveComponent(statusLabel, statusLabelBounds, 1.0f, animate);
        moveComponent(backButton, offscreenAbove(backButtonBounds), 0.0f, animate);
        moveComponent(resultsList, resultsNormalBounds, 1.0f, animate);
    }

    // The faded-out view must not intercept clicks meant for the visible one.
    promptEditor.setInterceptsMouseClicks(!shouldFocus, !shouldFocus);
    captureButton.setInterceptsMouseClicks(!shouldFocus, !shouldFocus);
    backButton.setInterceptsMouseClicks(shouldFocus, shouldFocus);
}

void ClauducerAudioProcessorEditor::onCaptureButtonClicked()
{
    auto capturedPath = audioProcessor.captureReferenceToTempFile(4.0);
    if (capturedPath.isEmpty())
    {
        setStatus("Nothing captured yet -- play some audio through this track first.", true);
        return;
    }

    audioProcessor.runSearch(capturedPath, promptEditor.getText());
}

void ClauducerAudioProcessorEditor::setStatus(const juce::String& text, bool isError)
{
    statusLabel.setText(text, juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, isError ? juce::Colours::orangered : juce::Colours::lightgreen);
}

void ClauducerAudioProcessorEditor::searchStarted()
{
    captureButton.setAnimating(true);
    setStatus("Analyzing and searching Splice...", false);
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
}
