#pragma once

#include "frontends/common2/commonframe.h"
#include "frontends/common2/speed.h"
#include <SDL.h>

struct EmulatorOptions;

class SDLFrame : public CommonFrame
{
public:
  SDLFrame(const EmulatorOptions & options);

  void VideoPresentScreen() override;
  void FrameRefreshStatus(int drawflags) override;
  int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

  void ProcessEvents(bool &quit);

  virtual void UpdateTexture() = 0;
  virtual void RenderPresent() = 0;

  const std::shared_ptr<SDL_Window> & GetWindow() const;

protected:
  void SetApplicationIcon();

  virtual void ProcessSingleEvent(const SDL_Event & event, bool & quit);

  void ProcessKeyDown(const SDL_KeyboardEvent & key);
  void ProcessKeyUp(const SDL_KeyboardEvent & key);
  void ProcessText(const SDL_TextInputEvent & text);

  std::shared_ptr<SDL_Window> myWindow;

  bool myForceCapsLock;
  int myMultiplier;
  bool myFullscreen;

  Speed mySpeed;
};
