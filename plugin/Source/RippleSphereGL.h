#pragma once

// RippleSphere.cpp is written against a glad-style global GL API (GLuint,
// glCreateShader, ...). JUCE 8 vendors the same OpenGL bindings but keeps
// them in the juce::gl namespace instead of the global one, so this shim
// pulls them into scope wherever it's used as RippleSphere's
// RIPPLESPHERE_GL_HEADER override -- no glad dependency needed inside a
// JUCE OpenGLContext.
#include <juce_opengl/juce_opengl.h>
using namespace ::juce::gl;
