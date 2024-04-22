#pragma once

#include "frontends/sdl/sdlframe.h"
#include <memory>

namespace sa2
{

  class SDLRendererFrame : public SDLFrame
  {
  public:
    SDLRendererFrame(const common2::EmulatorOptions & options);

    void VideoPresentScreen() override;
    void Initialize(bool resetVideoState) override;

    bool Quit() const override;

  protected:
    void GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, float & x, float & y) const override;
    void ToggleMouseCursor() override;

  private:

    static constexpr SDL_PixelFormatEnum ourPixelFormat = SDL_PIXELFORMAT_ARGB8888;

    SDL_Rect myRect;
    int myPitch;

    std::shared_ptr<SDL_Renderer> myRenderer;
    std::shared_ptr<SDL_Texture> myTexture;
  };

}
