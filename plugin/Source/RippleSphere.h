// RippleSphere.h
// A self-contained animated ripple sphere for OpenGL 3.3 core.
//
// The mesh is a parametric UV sphere. Only (u, v) is uploaded to the GPU;
// the vertex shader evaluates the wave, displaces the surface along its
// normal, and rebuilds the normal analytically so lighting follows the
// ripple instead of staying stuck to a smooth ball.
//
// Dependencies: an OpenGL 3.3 core context and a function loader.
// glad is assumed; point it elsewhere by defining RIPPLESPHERE_GL_HEADER
// before including this file, e.g.
//     #define RIPPLESPHERE_GL_HEADER <glew.h>
//
// Matrices are plain float[16] in OpenGL column-major order, so this has no
// math-library dependency. glm::value_ptr(m) works directly.

#pragma once

#ifndef RIPPLESPHERE_GL_HEADER
#define RIPPLESPHERE_GL_HEADER <glad/glad.h>
#endif
#include RIPPLESPHERE_GL_HEADER

#include <string>

class RippleSphere {
public:
    struct Params {
        // Geometry
        float radius    = 1.0f;   // base sphere radius
        float amplitude = 0.055f; // peak crest height, in world units
        float frequency = 16.0f;  // number of wave rings across the surface
        float speed     = 4.0f;   // ring travel speed
        float decay     = 0.35f;  // how fast rings fade away from the origin
        float secondary = 0.35f;  // weight of the slower cross-wave (0 = pure rings)

        // Direction on the unit sphere the rings radiate from
        float rippleDir[3] = { 0.0f, 1.0f, 0.0f };

        // Shading
        float lightDir[3]    = { -0.45f, -0.70f, -0.55f }; // direction light travels
        float colorTrough[3] = {  0.043f, 0.114f, 0.298f }; // deep water
        float colorCrest[3]  = {  0.180f, 0.686f, 0.784f }; // lit crest
        float rimColor[3]    = {  0.392f, 0.804f, 0.980f }; // fresnel edge glow
        float ambient        = 0.18f;
        float shininess      = 96.0f;
        float specularGain   = 0.55f;
        float rimGain        = 0.75f;
    };

    Params params;

    RippleSphere() = default;
    ~RippleSphere() { destroy(); }

    RippleSphere(const RippleSphere&)            = delete;
    RippleSphere& operator=(const RippleSphere&) = delete;

    // Builds the mesh and compiles the shaders. Needs a current GL context.
    // stacks = latitude rings, slices = longitude segments.
    // Returns false and fills errorOut (if given) when a shader fails.
    bool init(int stacks = 160, int slices = 320, std::string* errorOut = nullptr);

    // Releases every GL object. Safe to call more than once.
    void destroy();

    // Advances the animation clock. dt is in seconds.
    void update(float dt) { time_ += dt; }
    void  setTime(float t) { time_ = t; }
    float getTime() const  { return time_; }

    // Draws the sphere. Matrices are column-major float[16], cameraPos is float[3]
    // in world space. Binds its own program, VAO and uniforms; restores nothing,
    // so set your own state afterwards if you share the context.
    void draw(const float* model, const float* view, const float* proj,
              const float* cameraPos);

    unsigned int program() const { return program_; }
    int vertexCount() const   { return vertexCount_; }
    int triangleCount() const { return indexCount_ / 3; }

private:
    struct Uniforms {
        int model = -1, view = -1, proj = -1, normalMat = -1;
        int time = -1, radius = -1, amplitude = -1, frequency = -1;
        int speed = -1, decay = -1, secondary = -1, rippleDir = -1;
        int uvStep = -1;
        int camPos = -1, lightDir = -1;
        int colorTrough = -1, colorCrest = -1, rimColor = -1;
        int ambient = -1, shininess = -1, specularGain = -1, rimGain = -1;
    };

    void buildMesh(int stacks, int slices);
    void cacheUniforms();

    unsigned int vao_ = 0, vbo_ = 0, ebo_ = 0, program_ = 0;
    int   vertexCount_ = 0, indexCount_ = 0;
    float uvStep_[2]   = { 0.0f, 0.0f };
    float time_        = 0.0f;
    Uniforms u_;
};
