#include "PreviewPanel.h"
#include "AppFont.h"

namespace
{
    constexpr int kHeaderHeight = 32;
}

PreviewPanel::PreviewPanel()
{
    nameLabel.setFont(appFont(15.0f));
    nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1c2030));
    nameLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(nameLabel);

    openInBrowserButton.onClick = [this]
    {
        if (currentUrl.isNotEmpty())
            juce::URL(currentUrl).launchInDefaultBrowser();
    };
    addAndMakeVisible(openInBrowserButton);

    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton);
}

void PreviewPanel::show(const juce::String& sampleName, const juce::String& url)
{
    nameLabel.setText(sampleName, juce::dontSendNotification);
    currentUrl = url;

    if (browser == nullptr)
    {
        browser = std::make_unique<juce::WebBrowserComponent>();
        addAndMakeVisible(*browser);
        resized();
    }
    browser->goToURL(url);
    setVisible(true);
}

void PreviewPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xfff7f8fb));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
}

void PreviewPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(kHeaderHeight).reduced(4, 2);
    closeButton.setBounds(header.removeFromRight(kHeaderHeight));
    header.removeFromRight(4);
    openInBrowserButton.setBounds(header.removeFromRight(120));
    header.removeFromRight(8);
    nameLabel.setBounds(header);

    if (browser != nullptr)
        browser->setBounds(area.reduced(2));
}
