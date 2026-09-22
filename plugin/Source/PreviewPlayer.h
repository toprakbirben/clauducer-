#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

/** Plays a Splice result's preview with Splice's own web player, invisibly.

    Splice's preview audio is scrambled and only their page's player can
    play it, so play() loads the sound's Splice webpage in a hidden web view
    and, once the page is ready, presses its play button
    (button[data-qa="play-button"]) -- the same as the user clicking play on
    the page. Verified with a hidden WKWebView: the page's AudioContext runs
    and audio plays without the view ever being shown.

    Add as a hidden child (addChildComponent) with non-zero bounds; the web
    view is only created on the first play() so WebKit costs nothing until a
    preview is actually requested.
*/
class PreviewPlayer : public juce::Component
{
public:
    PreviewPlayer();
    ~PreviewPlayer() override;

    void play(const juce::String& url);
    void stop();

    void resized() override;

private:
    class Browser;
    std::unique_ptr<Browser> browser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewPlayer)
};
