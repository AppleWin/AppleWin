#pragma once

#if defined(IMGUI_IMPL_OPENGL_ES2)

// Pi3 with Fake KMS
// "OpenGL ES 2.0 Mesa 19.3.2"
// "Supported versions are: 1.00 ES"

#include <GLES2/gl2.h>
#define SDL_CONTEXT_MAJOR 2

#elif defined(IMGUI_IMPL_OPENGL_ES3)

// Pi4 with Fake KMS
// "OpenGL ES 3.1 Mesa 19.3.2"
// "Supported versions are: 1.00 ES, 3.00 ES, and 3.10 ES"

// On my desktop
// "OpenGL ES 3.1 Mesa 20.2.6"
// "Supported versions are: 1.00 ES, 3.00 ES, and 3.10 ES"

#include <GLES3/gl3.h>
#define SDL_CONTEXT_MAJOR 3
// "310 es" is accepted on a Pi4, but the imgui shaders do not compile

#endif

// this is used in all cases for GL_BGRA_EXT
#include <GLES2/gl2ext.h>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl3.h"
