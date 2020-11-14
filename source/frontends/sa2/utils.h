#pragma once

#include <SDL.h>
#include <memory>
#include <iosfwd>

void printRendererInfo(std::ostream & os, std::shared_ptr<SDL_Renderer> & ren, const Uint32 pixelFormat, const int selectedDriver);
