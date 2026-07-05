#pragma once

// ============================================================
// Platform-aware OpenGL include shim.
//
// Provides the correct GL / GLFW header include order and any
// fallback defines needed on Apple platforms.  Include this
// header instead of manually writing the platform #ifdef block.
// ============================================================

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GL_GLEXT_PROTOTYPES
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#include <OpenGL/glext.h>
// GL_TIME_ELAPSED is from GL_EXT_timer_query / GL_ARB_timer_query
#ifndef GL_TIME_ELAPSED
#define GL_TIME_ELAPSED 0x88BF
#endif
#ifndef GL_SAMPLES_PASSED
#define GL_SAMPLES_PASSED 0x8914
#endif
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif
