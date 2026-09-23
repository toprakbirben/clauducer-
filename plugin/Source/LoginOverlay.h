#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BackendClient.h"

/** Full-editor overlay shown when Splice isn't authorized (or the backend
    isn't running). check() queries GET /auth/status on a background thread
    and shows the overlay only if something is wrong; the button either
    retries (backend unreachable) or starts POST /auth/login and polls
    status every 2s until the browser login completes or times out.
*/
class LoginOverlay : public juce::Component, private juce::Thread
{
public:
    LoginOverlay();
    ~LoginOverlay() override;

    /** Re-checks auth; safe to call repeatedly (ignored while a check or login is running). */
    void check();

    /** Called when the overlay shows or hides. The editor uses it to hide the
        OpenGL capture button, whose native GL view would otherwise draw on top of this overlay. */
    std::function<void(bool isShowing)> onVisibilityChanged;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

private:
    enum class Mode { check, login };
    enum class State { checking, unreachable, unauthorized, loggingIn, authorized };

    void run() override;
    void startWorker(Mode);
    void post(State, const juce::String& detail); // from the worker thread
    void showState(State, const juce::String& detail);
    void onActionClicked();

    BackendClient backend;
    Mode mode = Mode::check;
    State state = State::checking;

    juce::Label titleLabel { {}, "Connect Splice" };
    juce::Label messageLabel;
    juce::TextButton actionButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginOverlay)
};
