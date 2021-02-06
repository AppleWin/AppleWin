#pragma once

#include "frontends/common2/commonframe.h"
#include <SDL.h>

class SDLFrame : public CommonFrame
{
public:
  SDLFrame();

  void VideoPresentScreen() override;
  void FrameRefreshStatus(int drawflags) override;
  int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

  virtual void UpdateTexture() = 0;
  virtual void RenderPresent() = 0;

  const std::shared_ptr<SDL_Window> & GetWindow() const;

protected:
  void SetApplicationIcon();

  std::shared_ptr<SDL_Window> myWindow;
};
