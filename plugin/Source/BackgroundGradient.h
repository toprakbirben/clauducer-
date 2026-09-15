#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Vivid multi-colour mesh gradient used as the editor's backdrop (coral
    upper-left, blue upper-right, violet lower-centre, over a bright base) --
    the "AI" look requested for the plugin background.

    Painted from the *full editor's* bounds so every child that needs to
    render its own slice of the same backdrop (namely CircularCaptureButton,
    which is opaque and must mask its own square GL corners) lines up
    seamlessly with the editor behind it. Pass:
      - areaToFill: the rectangle to paint, in the caller's local coordinates
      - editorBounds: the full editor's bounds, in editor coordinates
      - areaOriginInEditor: where the caller's local origin sits within the
        editor (0,0 if the caller *is* the editor)
*/
inline void paintAIGradientBackground(juce::Graphics& g,
                                       juce::Rectangle<float> areaToFill,
                                       juce::Rectangle<float> editorBounds,
                                       juce::Point<float> areaOriginInEditor)
{
    const juce::Colour base   { 0xfffaf8fb };
    const juce::Colour coral  { 0xffff9d66 };
    const juce::Colour blue   { 0xff5aa9f2 };
    const juce::Colour violet { 0xffb87bf0 };

    g.setColour(base);
    g.fillRect(areaToFill);

    const auto toLocal = [&](juce::Point<float> p) { return p - areaOriginInEditor; };

    const float w = editorBounds.getWidth();
    const float h = editorBounds.getHeight();
    const float radius = juce::jmax(w, h) * 0.65f;

    const auto blob = [&](juce::Colour c, juce::Point<float> centre, float alpha)
    {
        juce::ColourGradient glow(c.withAlpha(alpha), toLocal(centre),
                                   c.withAlpha(0.0f),  toLocal(centre.translated(radius, 0.0f)),
                                   true);
        g.setGradientFill(glow);
        g.fillRect(areaToFill);
    };

    blob(coral,  { w * 0.14f, h * 0.06f }, 0.62f);
    blob(blue,   { w * 0.88f, h * 0.10f }, 0.62f);
    blob(violet, { w * 0.55f, h * 0.60f }, 0.45f);
}
