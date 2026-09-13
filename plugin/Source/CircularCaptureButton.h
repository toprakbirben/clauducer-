#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <atomic>
#include <memory>

class RippleSphere;

/** The big circular "Capture & Search" button. Renders a RippleSphere (see
    RippleSphere.h/.cpp) through a fixed, front-on orthographic camera, so it
    reads as a flat glowing circular disc rather than a 3D ball -- ripples
    animate gently at rest and speed up while a search is in flight.
*/
class CircularCaptureButton : public juce::Component, private juce::OpenGLRenderer
{
public:
    CircularCaptureButton();
    ~CircularCaptureButton() override;

    std::function<void()> onClick;

    void setAnimating(bool shouldAnimate);

    // While locked, clicking no longer moves the ripple's origin or adds a
    // splash -- the idle/searching animation itself keeps running as normal.
    // Set true once the user clicks the button; call setLocked(false) when a
    // new prompt is entered to let clicks affect the ripple again.
    void setLocked(bool shouldLock);

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    juce::OpenGLContext glContext;
    std::unique_ptr<RippleSphere> sphere;
    std::atomic<bool> animating { false };
    double lastRenderTimeMs = 0.0;

    // Where the ripple originates -- a point on the sphere's visible
    // hemisphere, set from wherever the user last clicked (see mouseDown).
    // Defaults to the tuned upper-left touch point until the first click.
    std::atomic<float> rippleX { -0.706f }, rippleY { 0.709f }, rippleZ { 0.990f };

    // A brief amplitude boost that decays after each click, so a new ripple
    // reads as a fresh splash rather than the pattern silently recentring.
    std::atomic<double> clickPulseStartMs { -1.0e15 };

    // See setLocked().
    std::atomic<bool> locked { false };
};
