#include "PreviewPlayer.h"

namespace
{
    // Injected at document start of every full page load; survives the
    // site's in-app navigations (same document).
    const char* const kPageScript = R"JS(
window.clauducer = (() => {
  let targetId = '', wantPlay = false, timer = 0;
  const button = () => document.querySelector('button[data-qa="play-button"]');
  const idOf = (url) => (url.match(/\/sample\/([0-9a-f]+)/) || [])[1] || '';
  const isPlaying = () => { const b = button(); return !!b && b.innerText.trim() === 'Pause'; };
  // The button element is reused across in-app navigations, so it's only
  // "ready" for our sample once the address shows that sample's id.
  const readyButton = () => {
    const b = button();
    return b && !b.disabled && targetId && location.pathname.includes(targetId) ? b : null;
  };
  const tick = (tries) => {
    const b = readyButton();
    if (b) { if (wantPlay && !isPlaying()) b.click(); wantPlay = false; return; }
    if (tries < 300) timer = setTimeout(() => tick(tries + 1), 50);
    else wantPlay = false;  // gave up after ~15s
  };
  return {
    load(url, play) {
      clearTimeout(timer);
      if (isPlaying()) button().click();
      const id = idOf(url);
      const alreadyGoing = id === targetId;  // hovered, now clicked: don't restart the navigation
      targetId = id;
      wantPlay = play;
      if (!alreadyGoing && !location.pathname.includes(id)) {
        const a = document.createElement('a');
        a.href = url;
        document.body.appendChild(a);
        a.click();
        a.remove();
      }
      tick(0);
    },
    stop() { clearTimeout(timer); wantPlay = false; if (isPlaying()) button().click(); },
    state() { return isPlaying() ? 'playing' : (wantPlay ? 'pending' : 'idle'); },
  };
})();
)JS";

    constexpr int kStatePollMs = 250;
}

class PreviewPlayer::Browser : public juce::WebBrowserComponent
{
public:
    // By default JUCE blanks a hidden browser's page -- this one is never shown.
    explicit Browser(PreviewPlayer& ownerIn)
        : juce::WebBrowserComponent(Options{}
                                        .withKeepPageLoadedWhenBrowserIsHidden()
                                        .withUserScript(kPageScript)),
          owner(ownerIn)
    {
    }

    void pageFinishedLoading(const juce::String& url) override { owner.pageLoaded(url); }

private:
    PreviewPlayer& owner;
};

PreviewPlayer::PreviewPlayer() = default;
PreviewPlayer::~PreviewPlayer() = default;

void PreviewPlayer::prepare(const juce::String& url)
{
    if (!playing)
        request(url, false);
}

void PreviewPlayer::play(const juce::String& url)
{
    playing = true;
    ++playGeneration;
    request(url, true);
    startTimer(kStatePollMs);
}

void PreviewPlayer::stop()
{
    playing = false;
    ++playGeneration;
    pendingPlay = false;
    stopTimer();
    if (pageReady)
        browser->evaluateJavascript("clauducer.stop()");
}

void PreviewPlayer::request(const juce::String& url, bool shouldPlay)
{
    pendingUrl = url;
    pendingPlay = shouldPlay;

    if (browser == nullptr)
    {
        browser = std::make_unique<Browser>(*this);
        addAndMakeVisible(*browser);
        resized();
        browser->goToURL(url);
        return;
    }

    if (pageReady)
        browser->evaluateJavascript("clauducer.load(" + juce::JSON::toString(url) + ", "
                                    + (shouldPlay ? "true" : "false") + ")");
    // else: applied by pageLoaded() once the first page finishes loading.
}

void PreviewPlayer::pageLoaded(const juce::String& url)
{
    if (!url.startsWith("https://splice.com/"))
        return;

    pageReady = true;
    if (pendingUrl.isNotEmpty())
        request(pendingUrl, pendingPlay);
}

void PreviewPlayer::timerCallback()
{
    if (!pageReady)
        return; // still on the first full page load; play is applied once it finishes

    browser->evaluateJavascript("clauducer.state()",
                                [safeThis = juce::Component::SafePointer<PreviewPlayer>(this),
                                 generation = playGeneration](juce::WebBrowserComponent::EvaluationResult result)
                                {
                                    const auto* value = result.getResult();
                                    if (safeThis == nullptr || !safeThis->playing || safeThis->playGeneration != generation
                                        || value == nullptr || value->toString() != "idle")
                                        return;

                                    safeThis->playing = false;
                                    safeThis->stopTimer();
                                    if (safeThis->onFinished)
                                        safeThis->onFinished();
                                });
}

void PreviewPlayer::resized()
{
    if (browser != nullptr)
        browser->setBounds(getLocalBounds());
}
