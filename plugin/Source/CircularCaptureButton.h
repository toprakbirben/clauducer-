#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** The big circular "Capture & Search" button: a glowing orb that pulses
    gently at rest and animates a rotating arc + "Searching..." label while
    a search is in flight. Original design (not a copy of any reference
    image) -- captures a round, glowing, jewel-like feel via layered
    radial gradients and an animated stroke, done entirely with JUCE's
    Graphics API (no image assets).
*/
class CircularCaptureButton : public juce::Component, private juce::Timer
{
public:
    std::function<void()> onClick;

    void setAnimating(bool shouldAnimate)
    {
        animating = shouldAnimate;
        if (animating)
            startTimerHz(30);
        else
            stopTimer();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();

        // Outer glow.
        juce::ColourGradient glow(juce::Colour(0xff6f9dff).withAlpha(0.35f), centre,
                                   juce::Colour(0xff6f9dff).withAlpha(0.0f), centre.translated(radius, 0.0f), true);
        g.setGradientFill(glow);
        g.fillEllipse(bounds);

        // Core orb, breathing slightly at rest and shrinking a touch while animating.
        const float breathe = 0.03f * std::sin(phase * (animating ? 3.0f : 1.0f));
        auto orbBounds = bounds.reduced(radius * (0.12f - breathe));
        juce::ColourGradient orbGradient(juce::Colour(0xffeaf1ff), orbBounds.getCentre().translated(-radius * 0.25f, -radius * 0.25f),
                                          juce::Colour(0xff283968), orbBounds.getCentre().translated(radius * 0.35f, radius * 0.35f), true);
        g.setGradientFill(orbGradient);
        g.fillEllipse(orbBounds);

        if (animating)
        {
            juce::Path arc;
            const float startAngle = phase;
            arc.addArc(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                       startAngle, startAngle + juce::MathConstants<float>::pi * 0.7f, true);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawFittedText(animating ? "Searching..." : "Click to\nFind",
                          bounds.toNearestInt(), juce::Justification::centred, 2);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onClick && !animating)
            onClick();
    }

private:
    void timerCallback() override
    {
        phase += 0.12f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    bool animating = false;
    float phase = 0.0f;
};
