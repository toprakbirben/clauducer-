#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

/** Plays a Splice result's preview with Splice's own web player, invisibly.

    Splice's preview audio is scrambled and only their page's player can
    play it, so this keeps one Splice sample page loaded in a hidden web view
    and drives it with a small injected script (kPageScript):
      - switching sample uses the site's own in-app navigation (a link
        click, no page reload -- ~0.4-1.9s instead of ~3.5s for a full load);
      - playing presses the page's play button (button[data-qa="play-button"]),
        exactly as the user would;
      - the button's "Play"/"Pause" label tells us whether it's playing, so
        stop() presses it again and onFinished fires when a preview ends.
    Verified against live pages in a hidden WKWebView (the page's AudioContext
    runs; the right sample's audio is fetched).

    prepare() loads a sample without playing it (used on hover, and for the
    first result as soon as results arrive), so a later click only has to
    press play. It's ignored while a preview is playing, since switching the
    page's sample would cut it off.

    Add as a hidden child (addChildComponent) with non-zero bounds; the web
    view is only created on first use so WebKit costs nothing until then.
*/
class PreviewPlayer : public juce::Component,
                      private juce::Timer
{
public:
    PreviewPlayer();
    ~PreviewPlayer() override;

    void prepare(const juce::String& url);
    void play(const juce::String& url);
    void stop();

    /** Fired when a preview started with play() ends on its own, or fails
        to start. Not fired after stop(). */
    std::function<void()> onFinished;

    void resized() override;

private:
    class Browser;

    void request(const juce::String& url, bool shouldPlay);
    void pageLoaded(const juce::String& url);
    void timerCallback() override;

    std::unique_ptr<Browser> browser;
    bool pageReady = false; // the helper script is loaded and callable
    bool playing = false;   // between play() and stop()/onFinished
    int playGeneration = 0; // bumped by play()/stop() so a stale state poll can't end a newer preview
    juce::String pendingUrl;
    bool pendingPlay = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewPlayer)
};
