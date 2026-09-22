#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

/** In-plugin preview of a Splice result: its Splice webpage in an embedded
    web view, so the user plays it with Splice's own player without leaving
    the plugin. This is a normal page view (like the default browser), not
    scraping -- Splice's preview audio is scrambled and only their player
    can play it.

    The web view is a native view drawn above JUCE siblings, and is only
    created on the first show() so WebKit costs nothing until a preview is
    actually opened.
*/
class PreviewPanel : public juce::Component
{
public:
    PreviewPanel();

    void show(const juce::String& sampleName, const juce::String& url);

    std::function<void()> onClose;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label nameLabel;
    juce::TextButton openInBrowserButton { "Open in browser" };
    juce::TextButton closeButton { juce::CharPointer_UTF8("\xe2\x9c\x95") }; // "✕"
    std::unique_ptr<juce::WebBrowserComponent> browser;
    juce::String currentUrl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewPanel)
};
