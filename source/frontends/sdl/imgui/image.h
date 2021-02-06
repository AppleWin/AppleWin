#pragma once

#include <cstddef>
#include "frontends/sdl/imgui/gles.h"

void LoadTextureFromData(GLuint texture, const uint8_t * data, size_t width, size_t height, size_t pitch);
