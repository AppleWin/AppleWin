#pragma once

#include "frontends/sdl/sdlframe.h"

class EmulatorOptions;

class SDLRendererFrame : public SDLFrame
{
public:
  SDLRendererFrame(const EmulatorOptions & options);

  void UpdateTexture() override;
  void RenderPresent() override;

private:
  SDL_Rect myRect;
  int myPitch;

  std::shared_ptr<SDL_Renderer> myRenderer;
  std::shared_ptr<SDL_Texture> myTexture;
};
