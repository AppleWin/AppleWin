#pragma once

#include "Common.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/speed.h"
#include <SDL.h>

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  class SDLFrame : public common2::CommonFrame
  {
  public:
    SDLFrame(const common2::EmulatorOptions & options);

    void VideoPresentScreen() override;
    void FrameRefreshStatus(int drawflags) override;
    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
    void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

    void ProcessEvents(bool &quit);

    void Execute(const DWORD uCycles);
    void ExecuteOneFrame(const size_t msNextFrame);
    void ChangeMode(const AppMode_e mode);

    virtual void UpdateTexture() = 0;
    virtual void RenderPresent() = 0;

    const std::shared_ptr<SDL_Window> & GetWindow() const;

  protected:
    void SetApplicationIcon();

    virtual void ProcessSingleEvent(const SDL_Event & event, bool & quit);

    void ProcessKeyDown(const SDL_KeyboardEvent & key);
    void ProcessKeyUp(const SDL_KeyboardEvent & key);
    void ProcessText(const SDL_TextInputEvent & text);
    void ProcessDropEvent(const SDL_DropEvent & drop);

    std::shared_ptr<SDL_Window> myWindow;

    bool myForceCapsLock;
    int myMultiplier;
    bool myFullscreen;

    common2::Speed mySpeed;
  };

}
