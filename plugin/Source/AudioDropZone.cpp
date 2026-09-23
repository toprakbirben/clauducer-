#include "AudioDropZone.h"
#include "AppFont.h"

namespace
{
    const juce::Colour kBackground { 0xffeceff6 };
    const juce::Colour kBorder { 0xffd7dbe8 };
    const juce::Colour kHighlight { 0xff6f9dff };
    const juce::Colour kIdleIcon { 0xff8a90a6 };
    const juce::Colour kLoadedIcon { 0xff1f9d63 };
    const juce::Colour kTextPrimary { 0xff1c2030 };
    constexpr float kCornerRadius = 12.0f; // matches RoundedTextEditorLookAndFeel

    // Five rounded bars of varying height -- reads as "audio" at a glance.
    void drawWaveformIcon(juce::Graphics& g, juce::Rectangle<float> area)
    {
        constexpr float heights[] = { 0.4f, 0.75f, 1.0f, 0.6f, 0.3f };
        constexpr int numBars = (int) std::size(heights);
        const float slot = area.getWidth() / (float) numBars;
        const float barWidth = slot * 0.55f;
        for (int i = 0; i < numBars; ++i)
        {
            const float h = area.getHeight() * heights[i];
            g.fillRoundedRectangle(area.getX() + slot * (float) i + (slot - barWidth) * 0.5f,
                                   area.getCentreY() - h * 0.5f, barWidth, h, barWidth * 0.5f);
        }
    }
}

void AudioDropZone::setFileName(const juce::String& name)
{
    fileName = name;
    repaint();
}

void AudioDropZone::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(dragHovering ? kBackground.overlaidWith(kHighlight.withAlpha(0.15f)) : kBackground);
    g.fillRoundedRectangle(bounds, kCornerRadius);
    g.setColour(dragHovering ? kHighlight : kBorder);
    g.drawRoundedRectangle(bounds.reduced(0.5f), kCornerRadius, dragHovering ? 2.0f : 1.0f);

    auto content = bounds.reduced(8.0f);
    auto captionArea = content.removeFromBottom(16.0f);
    const float iconSize = juce::jmin(content.getWidth(), content.getHeight()) * 0.7f;
    g.setColour(dragHovering ? kHighlight : (fileName.isEmpty() ? kIdleIcon : kLoadedIcon));
    drawWaveformIcon(g, content.withSizeKeepingCentre(iconSize, iconSize));

    if (fileName.isNotEmpty())
    {
        auto x = clearButtonArea().reduced(4.0f);
        g.setColour(kIdleIcon);
        g.drawLine({ x.getTopLeft(), x.getBottomRight() }, 1.5f);
        g.drawLine({ x.getTopRight(), x.getBottomLeft() }, 1.5f);
    }

    g.setColour(kTextPrimary);
    g.setFont(appFont(12.0f));
    g.drawFittedText(fileName.isEmpty() ? "Drop audio" : fileName, captionArea.toNearestInt(),
                     juce::Justification::centred, 1, 0.8f);
}

juce::Rectangle<float> AudioDropZone::clearButtonArea() const
{
    return getLocalBounds().toFloat().removeFromTop(20.0f).removeFromRight(20.0f).translated(-3.0f, 3.0f);
}

void AudioDropZone::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mouseWasClicked())
        return;
    if (fileName.isNotEmpty() && clearButtonArea().contains(e.position))
    {
        if (onClear)
            onClear();
    }
    else if (onClick)
    {
        onClick();
    }
}

bool AudioDropZone::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1;
}

void AudioDropZone::fileDragEnter(const juce::StringArray&, int, int)
{
    dragHovering = true;
    repaint();
}

void AudioDropZone::fileDragExit(const juce::StringArray&)
{
    dragHovering = false;
    repaint();
}

void AudioDropZone::filesDropped(const juce::StringArray& files, int, int)
{
    dragHovering = false;
    repaint();
    if (onFileDropped)
        onFileDropped(juce::File(files[0]));
}
