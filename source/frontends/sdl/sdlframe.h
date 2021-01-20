#pragma once

#include "frontends/common2/commonframe.h"
#include <SDL.h>

class EmulatorOptions;

class SDLFrame : public CommonFrame
{
public:
  SDLFrame(const EmulatorOptions & options);

  virtual void VideoPresentScreen();
  virtual void FrameRefreshStatus(int drawflags);
  virtual int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType);
  virtual void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits);

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
