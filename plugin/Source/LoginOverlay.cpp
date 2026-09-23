#include "LoginOverlay.h"
#include "AppFont.h"
#include "BackgroundGradient.h"

namespace
{
    constexpr int kPollIntervalMs = 2000;
    // Slightly longer than the backend's own 180s wait for the OAuth callback.
    constexpr int kMaxLoginPolls = 95;
}

LoginOverlay::LoginOverlay() : juce::Thread("clauducer-auth")
{
    titleLabel.setFont(appFont(26.0f));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1c2030));
    addAndMakeVisible(titleLabel);

    messageLabel.setFont(appFont(15.0f));
    messageLabel.setJustificationType(juce::Justification::centredTop);
    messageLabel.setMinimumHorizontalScale(1.0f); // wrap instead of squashing onto one line
    messageLabel.setColour(juce::Label::textColourId, juce::Colour(0xff1c2030));
    addAndMakeVisible(messageLabel);

    actionButton.onClick = [this] { onActionClicked(); };
    addAndMakeVisible(actionButton);
}

LoginOverlay::~LoginOverlay()
{
    stopThread(6000);
}

void LoginOverlay::check()
{
    startWorker(Mode::check);
}

void LoginOverlay::startWorker(Mode newMode)
{
    if (isThreadRunning())
        return;
    mode = newMode;
    startThread();
}

void LoginOverlay::run()
{
    if (mode == Mode::login)
    {
        auto result = backend.startLogin();
        if (result.failed())
        {
            post(State::unreachable, result.getErrorMessage());
            return;
        }
        post(State::loggingIn, {});
    }

    const int maxPolls = mode == Mode::login ? kMaxLoginPolls : 1;
    for (int i = 0; i < maxPolls && !threadShouldExit(); ++i)
    {
        BackendClient::AuthStatus status;
        auto result = backend.authStatus(status);
        if (result.failed())
        {
            post(State::unreachable, result.getErrorMessage());
            return;
        }
        if (status.authorized)
        {
            post(State::authorized, {});
            return;
        }
        if (mode == Mode::check || !status.loginInProgress)
        {
            post(State::unauthorized, status.loginError.isNotEmpty() ? "Login failed: " + status.loginError : juce::String());
            return;
        }
        wait(kPollIntervalMs);
    }

    if (!threadShouldExit())
        post(State::unauthorized, "Timed out waiting for the browser login.");
}

void LoginOverlay::post(State newState, const juce::String& detail)
{
    juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<LoginOverlay>(this), newState, detail]
    {
        if (safeThis != nullptr)
            safeThis->showState(newState, detail);
    });
}

void LoginOverlay::showState(State newState, const juce::String& detail)
{
    state = newState;
    juce::String message;

    switch (newState)
    {
        case State::authorized:
            setVisible(false);
            return;
        case State::checking:
            message = "Checking Splice login...";
            break;
        case State::unreachable:
            message = "Can't reach the Clauducer backend. Start it with:\n\n"
                      "cd backend && uvicorn service:app --host 127.0.0.1 --port 8787";
            actionButton.setButtonText("Retry");
            break;
        case State::unauthorized:
            message = "Clauducer searches the Splice catalog for you. Log in once to connect your Splice account.";
            if (detail.isNotEmpty())
                message << "\n\n" << detail;
            actionButton.setButtonText("Log in to Splice");
            break;
        case State::loggingIn:
            message = "Finish logging in in your browser...";
            break;
    }

    messageLabel.setText(message, juce::dontSendNotification);
    actionButton.setVisible(newState == State::unreachable || newState == State::unauthorized);
    setVisible(true);
    toFront(false);
}

void LoginOverlay::onActionClicked()
{
    if (state == State::unreachable)
    {
        showState(State::checking, {});
        startWorker(Mode::check);
    }
    else if (state == State::unauthorized)
    {
        showState(State::loggingIn, {});
        startWorker(Mode::login);
    }
}

void LoginOverlay::visibilityChanged()
{
    if (onVisibilityChanged)
        onVisibilityChanged(isVisible());
}

void LoginOverlay::paint(juce::Graphics& g)
{
    paintAIGradientBackground(g, getLocalBounds().toFloat(), getLocalBounds().toFloat(), {});
}

void LoginOverlay::resized()
{
    auto area = getLocalBounds().reduced(40).withSizeKeepingCentre(getWidth() - 80, 260);
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);
    actionButton.setBounds(area.removeFromBottom(40).withSizeKeepingCentre(200, 40));
    area.removeFromBottom(10);
    messageLabel.setBounds(area);
}
