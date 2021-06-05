#pragma once

#include "frontends/sdl/sdlframe.h"

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  class SDLRendererFrame : public SDLFrame
  {
  public:
    SDLRendererFrame(const common2::EmulatorOptions & options);

    void UpdateTexture() override;
    void RenderPresent() override;

  protected:
    void GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const override;

  private:
    SDL_Rect myRect;
    int myPitch;

    std::shared_ptr<SDL_Renderer> myRenderer;
    std::shared_ptr<SDL_Texture> myTexture;
  };

}
