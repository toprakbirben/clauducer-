#include "CircularCaptureButton.h"
#include "BackgroundGradient.h"

#define RIPPLESPHERE_GL_HEADER "RippleSphereGL.h"
#include "RippleSphere.h"

#include <algorithm>
#include <cmath>

using namespace ::juce::gl;

namespace
{
    // The sphere sits at the origin with radius 1; the camera looks straight
    // down -Z through an orthographic frustum, so the disc doesn't
    // foreshorten -- no perspective, no orbit, just a flat circle with
    // ripple-driven shading (per the "should be a 2D object, not 3D" brief).
    constexpr float kCamDistance = 4.00f;
    constexpr float kViewRadius  = 1.17f; // headroom so ripple crests never hit the frustum edge

    constexpr float kIdleSpeed = 1.35f, kIdleAmplitude = 0.040f;
    constexpr float kSearchSpeed = 2.85f, kSearchAmplitude = 0.020f;

    constexpr float kSphereRadius = 1.0f;

    // A click's splash scales with how long it was held: a quick tap gives a
    // small, sharp ripple; a hold of kFullChargeSeconds or more gives the full
    // kClickPulseAmplitude with a long, slow decay. kClickPulseAmplitude is
    // also the cap on any boost (hold swell included) -- kMaxInwardDisplacement
    // below sizes the disc mask from it.
    constexpr float kClickPulseAmplitude = 0.06f;
    constexpr float kTapPulseAmplitude = 0.025f;
    constexpr float kTapPulseDecaySeconds = 0.18f;
    constexpr float kFullPulseDecaySeconds = 0.9f;
    constexpr double kFullChargeSeconds = 1.0;
    // While held, the surface swells gently towards the release.
    constexpr float kHoldSwellAmplitude = 0.035f;

    // A GL surface composites as opaque against the host window (its alpha
    // channel is ignored), so the square corners around the disc are masked
    // out in paint() with the same gradient PluginEditor paints behind it
    // (see BackgroundGradient.h), clipped to just that mask shape, rather
    // than left transparent or filled with a flat colour that would seam
    // against the gradient.

    void identity4(float* m)
    {
        std::fill(m, m + 16, 0.0f);
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    void ortho4(float* m, float halfW, float halfH, float n, float f)
    {
        identity4(m);
        m[0]  = 1.0f / halfW;
        m[5]  = 1.0f / halfH;
        m[10] = -2.0f / (f - n);
        m[14] = -(f + n) / (f - n);
    }

    void translateZ4(float* m, float z)
    {
        identity4(m);
        m[14] = z;
    }
}

CircularCaptureButton::CircularCaptureButton()
{
    glContext.setRenderer(this);
    glContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    glContext.setContinuousRepainting(true);
    glContext.attachTo(*this);
}

CircularCaptureButton::~CircularCaptureButton()
{
    glContext.detach();
}

void CircularCaptureButton::setAnimating(bool shouldAnimate)
{
    animating = shouldAnimate;
    repaint();
}

void CircularCaptureButton::setLocked(bool shouldLock)
{
    locked = shouldLock;
}

void CircularCaptureButton::newOpenGLContextCreated()
{
    sphere = std::make_unique<RippleSphere>();
    sphere->params.speed     = kIdleSpeed;
    sphere->params.amplitude = kIdleAmplitude;

    sphere->params.radius    = kSphereRadius;
    sphere->params.frequency = 8.0f;
    sphere->params.decay     = 1.000f;
    sphere->params.secondary = 0.400f;

    sphere->params.lightDir[0] = -0.290f;
    sphere->params.lightDir[1] = -0.660f;
    sphere->params.lightDir[2] = -0.990f;

    sphere->params.colorTrough[0] = 0.750f;  sphere->params.colorTrough[1] = 0.870f;  sphere->params.colorTrough[2] = 0.960f;
    sphere->params.colorCrest[0]  = 0.800f;  sphere->params.colorCrest[1]  = 0.740f;  sphere->params.colorCrest[2]  = 0.930f;
    sphere->params.rimColor[0]    = 0.380f;  sphere->params.rimColor[1]    = 0.550f;  sphere->params.rimColor[2]    = 0.760f;

    sphere->params.ambient      = 0.210f;
    sphere->params.shininess    = 164.0f;
    sphere->params.specularGain = 0.000f;
    sphere->params.rimGain      = 0.300f;

    std::string error;
    if (!sphere->init(40, 80, &error))
    {
        jassertfalse;
        sphere.reset();
    }

    lastRenderTimeMs = juce::Time::getMillisecondCounterHiRes();
}

void CircularCaptureButton::renderOpenGL()
{
    if (sphere == nullptr)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit(0.0, 0.1, (now - lastRenderTimeMs) / 1000.0);
    lastRenderTimeMs = now;

    sphere->params.rippleDir[0] = kRippleX;
    sphere->params.rippleDir[1] = kRippleY;
    sphere->params.rippleDir[2] = kRippleZ;

    const bool searching = animating.load();
    sphere->params.speed = searching ? kSearchSpeed : kIdleSpeed;

    // The idle/searching ripple always keeps running; "locked" (see
    // setLocked) only gates whether a click can move the origin or add a
    // fresh splash -- it never stops the animation itself.
    const float pulseAge = (float) juce::jmax(0.0, (now - clickPulseStartMs.load()) / 1000.0);
    float boost = pulseAmplitude.load() * std::exp(-pulseAge / pulseDecaySeconds.load());
    if (holding.load())
        boost = juce::jmax(boost, kHoldSwellAmplitude * chargeAt(now));
    sphere->params.amplitude = (searching ? kSearchAmplitude : kIdleAmplitude)
                             + juce::jmin(boost, kClickPulseAmplitude);
    sphere->update(dt);

    const auto scale = (float) glContext.getRenderingScale();
    const auto bounds = getLocalBounds();
    const int w = juce::jmax(1, juce::roundToInt((float) bounds.getWidth() * scale));
    const int h = juce::jmax(1, juce::roundToInt((float) bounds.getHeight() * scale));

    // Clearing to black would show through as a dark fringe where the GL
    // surface's antialiased circular edge doesn't pixel-perfectly line up
    // with the mask ellipse paint() draws around it (see paint()) -- clear
    // to a light neutral matching the surrounding gradient's base tone
    // instead, so that seam disappears into the backdrop rather than
    // reading as a hard black ring.
    glViewport(0, 0, w, h);
    glClearColor(0.965f, 0.955f, 0.975f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    const float aspect = (float) w / (float) h;
    const float halfW = aspect >= 1.0f ? kViewRadius * aspect : kViewRadius;
    const float halfH = aspect >= 1.0f ? kViewRadius : kViewRadius / aspect;

    float model[16], view[16], proj[16];
    identity4(model);
    translateZ4(view, -kCamDistance);
    ortho4(proj, halfW, halfH, 0.01f, 10.0f);

    const float camPos[3] = { 0.0f, 0.0f, kCamDistance };
    sphere->draw(model, view, proj, camPos);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void CircularCaptureButton::openGLContextClosing()
{
    sphere.reset();
}

void CircularCaptureButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Crop the square GL viewport down to the disc the sphere actually
    // occupies (radius 1 world unit out of the kViewRadius half-frustum),
    // filling everything outside it so no black square shows through.
    //
    // The ripple displaces the mesh radially by up to ~kMaxInwardDisplacement
    // world units (idle wobble plus a click's splash pulse, each further
    // scaled by the shader's secondary-band term -- see waveHeight() in
    // RippleSphere.cpp), so at trough phases the sphere's true silhouette
    // pulls in from its radius-1 rest shape. Sizing the mask to the sphere's
    // *rest* radius would then expose a sliver of raw GL clear colour at the
    // rim on every trough; instead the mask is sized to the sphere's
    // guaranteed-safe minimum radius, so it always sits at or inside the
    // rendered silhouette (crests simply run a little past the mask, which
    // just reads as part of the sphere rather than a gap).
    constexpr float kMaxInwardDisplacement = (kIdleAmplitude + kClickPulseAmplitude) * 1.2f;
    const float minSide = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float circleRadius = minSide * 0.5f / kViewRadius * (kSphereRadius - kMaxInwardDisplacement) / kSphereRadius;
    auto circleBounds = juce::Rectangle<float>(circleRadius * 2.0f, circleRadius * 2.0f)
                             .withCentre(bounds.getCentre());

    juce::Path mask;
    mask.addRectangle(bounds);
    mask.addEllipse(circleBounds);
    mask.setUsingNonZeroWinding(false);

    auto* editor = getParentComponent();
    const auto editorBounds = editor != nullptr ? editor->getLocalBounds().toFloat() : bounds;
    const auto originInEditor = getBounds().getTopLeft().toFloat();

    g.saveState();
    g.reduceClipRegion(mask);
    paintAIGradientBackground(g, bounds, editorBounds, originInEditor);
    g.restoreState();

    // A soft, low-key white rim right at the disc's edge -- a gentle glow
    // that eases the sphere into its surroundings rather than a hard cutout.
    {
        const juce::Colour rim = juce::Colours::white;
        const float rimWidth = circleRadius * 0.10f;
        const float rimStart = juce::jlimit(0.0f, 1.0f, 1.0f - rimWidth / circleRadius);

        juce::ColourGradient rimGlow(rim.withAlpha(0.0f), bounds.getCentre(),
                                      rim.withAlpha(0.22f), bounds.getCentre().translated(circleRadius, 0.0f),
                                      true);
        rimGlow.addColour((double) rimStart, rim.withAlpha(0.0f));
        g.setGradientFill(rimGlow);
        g.fillEllipse(circleBounds);
    }

    // No label drawn over the disc -- the status label below the button
    // already carries state ("Analyzing and searching Splice...", error
    // text, result count), so the sphere itself stays clean in every state.
}

void CircularCaptureButton::mouseDown(const juce::MouseEvent&)
{
    if (locked.load())
        return;

    // The ripple always originates from the fixed back-left/top point (see
    // kRippleX/Y/Z) -- a press only swells it and its release adds a splash
    // pulse, it doesn't move the origin.
    pressStartMs = juce::Time::getMillisecondCounterHiRes();
    holding = true;
}

float CircularCaptureButton::chargeAt(double nowMs) const
{
    return (float) juce::jlimit(0.0, 1.0, (nowMs - pressStartMs.load()) / 1000.0 / kFullChargeSeconds);
}

void CircularCaptureButton::mouseUp(const juce::MouseEvent&)
{
    if (holding.exchange(false))
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const float charge = chargeAt(now);
        pulseAmplitude = juce::jmap(charge, kTapPulseAmplitude, kClickPulseAmplitude);
        pulseDecaySeconds = juce::jmap(charge, kTapPulseDecaySeconds, kFullPulseDecaySeconds);
        clickPulseStartMs = now;
    }

    if (onClick && !animating.load())
    {
        locked = true;
        onClick();
    }
}
