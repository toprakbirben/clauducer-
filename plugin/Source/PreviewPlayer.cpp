#include "PreviewPlayer.h"

namespace
{
    // Polls until the page's play button is hydrated (it renders disabled
    // first), then presses it once. Gives up after ~15s.
    const char* const kAutoplayScript = R"JS(
(() => {
  if (window.__clauducerAutoplay) return;
  window.__clauducerAutoplay = true;
  let tries = 0;
  const timer = setInterval(() => {
    const button = document.querySelector('button[data-qa="play-button"]');
    if (button && !button.disabled) { clearInterval(timer); button.click(); }
    else if (++tries > 150) clearInterval(timer);
  }, 100);
})();
)JS";
}

class PreviewPlayer::Browser : public juce::WebBrowserComponent
{
public:
    // By default JUCE blanks a hidden browser's page -- this one is never shown.
    Browser() : juce::WebBrowserComponent(Options{}.withKeepPageLoadedWhenBrowserIsHidden()) {}

    void pageFinishedLoading(const juce::String& url) override
    {
        if (url.startsWith("https://splice.com/"))
            evaluateJavascript(kAutoplayScript);
    }
};

PreviewPlayer::PreviewPlayer() = default;
PreviewPlayer::~PreviewPlayer() = default;

void PreviewPlayer::play(const juce::String& url)
{
    if (browser == nullptr)
    {
        browser = std::make_unique<Browser>();
        addAndMakeVisible(*browser);
        resized();
    }
    browser->goToURL(url);
}

void PreviewPlayer::stop()
{
    if (browser != nullptr)
        browser->goToURL("about:blank");
}

void PreviewPlayer::resized()
{
    if (browser != nullptr)
        browser->setBounds(getLocalBounds());
}
