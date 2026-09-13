#include "CircularCaptureButton.h"

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

    // How hard a click's splash boosts amplitude, and how fast that boost
    // decays back to the idle/searching level.
    constexpr float kClickPulseAmplitude = 0.06f;
    constexpr float kClickPulseDecaySeconds = 0.35f;

    // Must match PluginEditor's kBackground -- a GL surface composites as opaque
    // against the host window (its alpha channel is ignored), so the square
    // corners around the disc are masked out with an explicit fill in paint()
    // rather than left transparent.
    const juce::Colour kPanelBackground { 0xff0b0d14 };

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

    sphere->params.colorTrough[0] = 0.541f;  sphere->params.colorTrough[1] = 0.863f;  sphere->params.colorTrough[2] = 1.000f;
    sphere->params.colorCrest[0]  = 1.000f;  sphere->params.colorCrest[1]  = 1.000f;  sphere->params.colorCrest[2]  = 1.000f;
    sphere->params.rimColor[0]    = 0.459f;  sphere->params.rimColor[1]    = 0.600f;  sphere->params.rimColor[2]    = 0.663f;

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

    sphere->params.rippleDir[0] = rippleX.load();
    sphere->params.rippleDir[1] = rippleY.load();
    sphere->params.rippleDir[2] = rippleZ.load();

    const bool searching = animating.load();
    sphere->params.speed = searching ? kSearchSpeed : kIdleSpeed;

    // The idle/searching ripple always keeps running; "locked" (see
    // setLocked) only gates whether a click can move the origin or add a
    // fresh splash -- it never stops the animation itself.
    const float pulseAge = (float) juce::jmax(0.0, (now - clickPulseStartMs.load()) / 1000.0);
    const float pulseBoost = kClickPulseAmplitude * std::exp(-pulseAge / kClickPulseDecaySeconds);
    sphere->params.amplitude = (searching ? kSearchAmplitude : kIdleAmplitude) + pulseBoost;
    sphere->update(dt);

    const auto scale = (float) glContext.getRenderingScale();
    const auto bounds = getLocalBounds();
    const int w = juce::jmax(1, juce::roundToInt((float) bounds.getWidth() * scale));
    const int h = juce::jmax(1, juce::roundToInt((float) bounds.getHeight() * scale));

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
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
    const float minSide = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const float circleRadius = minSide * 0.5f / kViewRadius;
    auto circleBounds = juce::Rectangle<float>(circleRadius * 2.0f, circleRadius * 2.0f)
                             .withCentre(bounds.getCentre());

    juce::Path mask;
    mask.addRectangle(bounds);
    mask.addEllipse(circleBounds);
    mask.setUsingNonZeroWinding(false);
    g.setColour(kPanelBackground);
    g.fillPath(mask);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawFittedText(animating.load() ? "Searching..." : "Click to\nFind",
                      getLocalBounds(), juce::Justification::centred, 2);
}

void CircularCaptureButton::mouseDown(const juce::MouseEvent& e)
{
    if (locked.load())
        return;

    // Map the click from screen space into the same front-on orthographic
    // frame the sphere is rendered through (see renderOpenGL/paint), then
    // reconstruct the point on the visible hemisphere directly beneath the
    // cursor -- nz completes the unit direction, same as a standard
    // normal-mapped disc.
    const auto bounds = getLocalBounds().toFloat();
    const float halfSidePx = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();

    const float worldPerPixel = kViewRadius / halfSidePx;
    float nx = (e.position.x - centre.x) * worldPerPixel / kSphereRadius;
    float ny = -(e.position.y - centre.y) * worldPerPixel / kSphereRadius; // screen Y is flipped vs. world up

    const float d2 = nx * nx + ny * ny;
    if (d2 > 1.0f)
    {
        const float scale = 1.0f / std::sqrt(d2);
        nx *= scale;
        ny *= scale;
    }
    const float nz = std::sqrt(juce::jmax(0.0f, 1.0f - nx * nx - ny * ny));

    rippleX = nx;
    rippleY = ny;
    rippleZ = nz;
    clickPulseStartMs = juce::Time::getMillisecondCounterHiRes();
}

void CircularCaptureButton::mouseUp(const juce::MouseEvent&)
{
    if (onClick && !animating.load())
    {
        locked = true;
        onClick();
    }
}
