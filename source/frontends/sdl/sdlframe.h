#pragma once

#include "linux/linuxframe.h"
#include <SDL.h>

class EmulatorOptions;

class SDLFrame : public LinuxFrame
{
public:
  SDLFrame(const EmulatorOptions & options);

  virtual void VideoPresentScreen();
  virtual void FrameRefreshStatus(int drawflags);

  void UpdateTexture();
  void RenderPresent();

  const std::shared_ptr<SDL_Window> & GetWindow() const;

private:
  void SetApplicationIcon();

  SDL_Rect myRect;
  int myPitch;

  std::shared_ptr<SDL_Window> myWindow;
  std::shared_ptr<SDL_Renderer> myRenderer;
  std::shared_ptr<SDL_Texture> myTexture;
};
