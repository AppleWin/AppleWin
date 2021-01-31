#pragma once

#include "frontends/common2/commonframe.h"
#include <SDL.h>

class EmulatorOptions;

class SDLFrame : public CommonFrame
{
public:
  SDLFrame(const EmulatorOptions & options);

  void VideoPresentScreen() override;
  void FrameRefreshStatus(int drawflags) override;
  int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

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
