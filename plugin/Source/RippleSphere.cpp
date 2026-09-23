// RippleSphere.cpp
#define RIPPLESPHERE_GL_HEADER "RippleSphereGL.h"
#include "RippleSphere.h"

#include <vector>
#include <cmath>

namespace {

const char* kVertexSrc = R"GLSL(
#version 330 core

// The only per-vertex data is the sphere's parameter pair.
// x = longitude in [0,1], y = latitude in [0,1] (0 = north pole).
layout(location = 0) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

uniform float uTime;
uniform float uRadius;
uniform float uAmplitude;
uniform float uFrequency;
uniform float uSpeed;
uniform float uDecay;
uniform float uSecondary;
uniform vec3  uRippleDir;
uniform vec2  uUVStep;   // finite-difference step, a quarter of a mesh cell

out vec3  vWorldPos;
out vec3  vNormal;
out float vHeight;

const float PI = 3.141592653589793;

vec3 unitSphere(vec2 uv)
{
    float phi   = uv.x * 2.0 * PI;
    float theta = uv.y * PI;
    float st    = sin(theta);
    return vec3(st * cos(phi), cos(theta), st * sin(phi));
}

// Height field evaluated on the unit sphere direction.
// Rings travel outward from uRippleDir along the great-circle angle, so the
// wave wraps the surface correctly instead of sliding in UV space.
float waveHeight(vec3 dir)
{
    float ang = acos(clamp(dot(dir, normalize(uRippleDir)), -1.0, 1.0));

    float rings = sin(ang * uFrequency - uTime * uSpeed) * exp(-ang * uDecay);

    // A slower counter-travelling band keeps the motion from looking like a
    // perfect, mechanical pond.
    float band = sin(dir.y * uFrequency * 0.5 + uTime * uSpeed * 0.6) * 0.5;

    return uAmplitude * (rings + uSecondary * band);
}

vec3 displaced(vec2 uv, out float h)
{
    vec3 dir = unitSphere(uv);
    h = waveHeight(dir);
    return dir * (uRadius + h);
}

void main()
{
    float h;
    vec3 p = displaced(aUV, h);

    // Rebuild the normal from the displaced surface: sample two neighbours in
    // parameter space and cross the resulting tangents. Without this the
    // lighting stays smooth and the ripple reads as a silhouette wobble only.
    float eu = uUVStep.x;
    float ev = (aUV.y + uUVStep.y > 1.0) ? -uUVStep.y : uUVStep.y;

    float hu, hv;
    vec3 pu = displaced(aUV + vec2(eu, 0.0), hu);
    vec3 pv = displaced(aUV + vec2(0.0, ev), hv);

    vec3 c = cross(pu - p, pv - p) * sign(ev);
    vec3 n = (dot(c, c) > 1e-14) ? normalize(c) : normalize(p);

    vec4 world = uModel * vec4(p, 1.0);
    vWorldPos  = world.xyz;
    vNormal    = uNormalMat * n;
    vHeight    = h;

    gl_Position = uProj * uView * world;
}
)GLSL";

const char* kFragmentSrc = R"GLSL(
#version 330 core

in vec3  vWorldPos;
in vec3  vNormal;
in float vHeight;

out vec4 FragColor;

uniform vec3  uCamPos;
uniform vec3  uLightDir;
uniform vec3  uColorTrough;
uniform vec3  uColorCrest;
uniform vec3  uRimColor;
uniform float uAmbient;
uniform float uShininess;
uniform float uSpecularGain;
uniform float uRimGain;
uniform float uAmplitude;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(-uLightDir);

    float diff = max(dot(N, L), 0.0);
    vec3  H    = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess);

    // Crests read lighter than troughs, which is what sells the depth.
    float t    = clamp(vHeight / max(uAmplitude * 1.6, 1e-5) * 0.5 + 0.5, 0.0, 1.0);
    vec3  base = mix(uColorTrough, uColorCrest, t * t);

    float fres = pow(1.0 - max(dot(N, V), 0.0), 3.0);

    vec3 color = base * (uAmbient + (1.0 - uAmbient) * diff)
               + vec3(1.0) * spec * uSpecularGain
               + uRimColor * fres * uRimGain;

    FragColor = vec4(color, 1.0);
}
)GLSL";

GLuint compileStage(GLenum type, const char* src, std::string* errorOut)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);

    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetShaderInfoLog(s, len, nullptr, &log[0]);
        if (errorOut) {
            *errorOut = (type == GL_VERTEX_SHADER ? "vertex shader: " : "fragment shader: ") + log;
        }
        glDeleteShader(s);
        return 0;
    }
    return s;
}

// Normal matrix = transpose(inverse(upper-left 3x3 of model)), column-major out.
void normalMatrix3(const float* m, float* out)
{
    const float a00 = m[0], a10 = m[1], a20 = m[2];
    const float a01 = m[4], a11 = m[5], a21 = m[6];
    const float a02 = m[8], a12 = m[9], a22 = m[10];

    const float c00 =  (a11 * a22 - a12 * a21);
    const float c01 = -(a10 * a22 - a12 * a20);
    const float c02 =  (a10 * a21 - a11 * a20);
    const float c10 = -(a01 * a22 - a02 * a21);
    const float c11 =  (a00 * a22 - a02 * a20);
    const float c12 = -(a00 * a21 - a01 * a20);
    const float c20 =  (a01 * a12 - a02 * a11);
    const float c21 = -(a00 * a12 - a02 * a10);
    const float c22 =  (a00 * a11 - a01 * a10);

    float det = a00 * c00 + a01 * c01 + a02 * c02;
    float inv = (std::fabs(det) > 1e-12f) ? 1.0f / det : 0.0f;

    // out[col * 3 + row] = cofactor(row, col) / det
    out[0] = c00 * inv; out[1] = c10 * inv; out[2] = c20 * inv;
    out[3] = c01 * inv; out[4] = c11 * inv; out[5] = c21 * inv;
    out[6] = c02 * inv; out[7] = c12 * inv; out[8] = c22 * inv;
}

} // namespace

bool RippleSphere::init(int stacks, int slices, std::string* errorOut)
{
    destroy();

    if (stacks < 3)  stacks = 3;
    if (slices < 3)  slices = 3;

    GLuint vs = compileStage(GL_VERTEX_SHADER, kVertexSrc, errorOut);
    if (!vs) return false;
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, kFragmentSrc, errorOut);
    if (!fs) { glDeleteShader(vs); return false; }

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint len = 0;
        glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len > 1 ? len : 1), '\0');
        glGetProgramInfoLog(program_, len, nullptr, &log[0]);
        if (errorOut) *errorOut = "link: " + log;
        glDeleteShader(vs);
        glDeleteShader(fs);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    buildMesh(stacks, slices);
    cacheUniforms();
    return true;
}

void RippleSphere::buildMesh(int stacks, int slices)
{
    std::vector<float>    uvs;
    std::vector<unsigned> indices;

    uvs.reserve(static_cast<size_t>(stacks + 1) * static_cast<size_t>(slices + 1) * 2);
    indices.reserve(static_cast<size_t>(stacks) * static_cast<size_t>(slices) * 6);

    for (int i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(stacks);
        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(slices);
            uvs.push_back(u);
            uvs.push_back(v);
        }
    }

    const int rowLen = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned a = static_cast<unsigned>(i * rowLen + j);
            unsigned b = a + 1;                                  // +u
            unsigned c = static_cast<unsigned>((i + 1) * rowLen + j); // +v
            unsigned d = c + 1;

            // CCW when seen from outside.
            indices.push_back(a); indices.push_back(b); indices.push_back(c);
            indices.push_back(b); indices.push_back(d); indices.push_back(c);
        }
    }

    vertexCount_ = static_cast<int>(uvs.size() / 2);
    indexCount_  = static_cast<int>(indices.size());
    // A quarter of a cell: small enough that normals track the wave closely
    // (under ~2 degrees of error at default frequency), large enough to stay
    // clear of float precision trouble in the shader.
    uvStep_[0]   = 0.25f / static_cast<float>(slices);
    uvStep_[1]   = 0.25f / static_cast<float>(stacks);

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(uvs.size() * sizeof(float)),
                 uvs.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned)),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    glBindVertexArray(0);
}

void RippleSphere::cacheUniforms()
{
    auto loc = [this](const char* n) { return glGetUniformLocation(program_, n); };

    u_.model        = loc("uModel");
    u_.view         = loc("uView");
    u_.proj         = loc("uProj");
    u_.normalMat    = loc("uNormalMat");
    u_.time         = loc("uTime");
    u_.radius       = loc("uRadius");
    u_.amplitude    = loc("uAmplitude");
    u_.frequency    = loc("uFrequency");
    u_.speed        = loc("uSpeed");
    u_.decay        = loc("uDecay");
    u_.secondary    = loc("uSecondary");
    u_.rippleDir    = loc("uRippleDir");
    u_.uvStep       = loc("uUVStep");
    u_.camPos       = loc("uCamPos");
    u_.lightDir     = loc("uLightDir");
    u_.colorTrough  = loc("uColorTrough");
    u_.colorCrest   = loc("uColorCrest");
    u_.rimColor     = loc("uRimColor");
    u_.ambient      = loc("uAmbient");
    u_.shininess    = loc("uShininess");
    u_.specularGain = loc("uSpecularGain");
    u_.rimGain      = loc("uRimGain");
}

void RippleSphere::draw(const float* model, const float* view, const float* proj,
                        const float* cameraPos)
{
    if (!program_ || !vao_) return;

    float nrm[9];
    normalMatrix3(model, nrm);

    glUseProgram(program_);

    glUniformMatrix4fv(u_.model, 1, GL_FALSE, model);
    glUniformMatrix4fv(u_.view,  1, GL_FALSE, view);
    glUniformMatrix4fv(u_.proj,  1, GL_FALSE, proj);
    glUniformMatrix3fv(u_.normalMat, 1, GL_FALSE, nrm);

    glUniform1f(u_.time,      time_);
    glUniform1f(u_.radius,    params.radius);
    glUniform1f(u_.amplitude, params.amplitude);
    glUniform1f(u_.frequency, params.frequency);
    glUniform1f(u_.speed,     params.speed);
    glUniform1f(u_.decay,     params.decay);
    glUniform1f(u_.secondary, params.secondary);
    glUniform3fv(u_.rippleDir, 1, params.rippleDir);
    glUniform2f(u_.uvStep, uvStep_[0], uvStep_[1]);

    glUniform3fv(u_.camPos,      1, cameraPos);
    glUniform3fv(u_.lightDir,    1, params.lightDir);
    glUniform3fv(u_.colorTrough, 1, params.colorTrough);
    glUniform3fv(u_.colorCrest,  1, params.colorCrest);
    glUniform3fv(u_.rimColor,    1, params.rimColor);
    glUniform1f(u_.ambient,      params.ambient);
    glUniform1f(u_.shininess,    params.shininess);
    glUniform1f(u_.specularGain, params.specularGain);
    glUniform1f(u_.rimGain,      params.rimGain);

    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void RippleSphere::destroy()
{
    if (ebo_)     { glDeleteBuffers(1, &ebo_);           ebo_ = 0; }
    if (vbo_)     { glDeleteBuffers(1, &vbo_);           vbo_ = 0; }
    if (vao_)     { glDeleteVertexArrays(1, &vao_);      vao_ = 0; }
    if (program_) { glDeleteProgram(program_);       program_ = 0; }
    vertexCount_ = indexCount_ = 0;
}
