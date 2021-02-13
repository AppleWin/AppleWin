#pragma once

#include <cstddef>
#include "frontends/sdl/imgui/gles.h"

void allocateTexture(GLuint texture, size_t width, size_t height);
void loadTextureFromData(GLuint texture, const uint8_t * data, size_t width, size_t height, size_t pitch);
