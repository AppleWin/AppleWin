#pragma once

#include "frontends/sdl/sdlcompat.h"
#include <memory>
#include <iosfwd>
#include <string>

namespace sa2
{

    void printVideoInfo(std::ostream &os);

    void printAudioInfo(std::ostream &os);

    void printRendererInfo(
        std::ostream &os, const std::shared_ptr<SDL_Renderer> &ren, const PixelFormat_t pixelFormat,
        const int selectedDriver);

    bool show_yes_no_dialog(const std::shared_ptr<SDL_Window> &win, const std::string &title, const std::string &text);

    std::string decorateSDLError(const std::string &prefix);

    size_t getCanonicalModifiers(const SDL_KeyboardEvent &key);

} // namespace sa2
