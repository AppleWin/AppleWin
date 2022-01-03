#pragma once

#include "Common.h"
#include "Configuration/Config.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/speed.h"
#include <SDL.h>

namespace common2
{
  struct EmulatorOptions;
}

namespace sa2
{

  class SDLFrame : public virtual common2::CommonFrame
  {
  public:
    SDLFrame(const common2::EmulatorOptions & options);

    void Begin() override;
    void End() override;

    void FrameRefreshStatus(int drawflags) override;
    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
    void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

    void ProcessEvents(bool &quit);

    void ExecuteOneFrame(const size_t msNextFrame);
    void ChangeMode(const AppMode_e mode);
    void SingleStep();
    void ResetHardware();
    bool HardwareChanged() const;
    virtual void ResetSpeed();

    const std::shared_ptr<SDL_Window> & GetWindow() const;

    void getDragDropSlotAndDrive(size_t & slot, size_t & drive) const;
    void setDragDropSlotAndDrive(const size_t slot, const size_t drive);

    static void setGLSwapInterval(const int interval);

  protected:
    void SetApplicationIcon();

    virtual void ProcessSingleEvent(const SDL_Event & event, bool & quit);
    virtual void GetRelativeMousePosition(const SDL_MouseMotionEvent & motion, double & x, double & y) const = 0;

    void ProcessKeyDown(const SDL_KeyboardEvent & key);
    void ProcessKeyUp(const SDL_KeyboardEvent & key);
    void ProcessText(const SDL_TextInputEvent & text);
    void ProcessDropEvent(const SDL_DropEvent & drop);
    void ProcessMouseButton(const SDL_MouseButtonEvent & button);
    void ProcessMouseMotion(const SDL_MouseMotionEvent & motion);

    void ExecuteInRunningMode(const size_t msNextFrame);
    void ExecuteInDebugMode(const size_t msNextFrame);
    void Execute(const DWORD uCycles);

    void SetFullSpeed(const bool value);
    bool CanDoFullSpeed();

    static double GetRelativePosition(const int value, const int width);

    int myTargetGLSwap;
    bool myForceCapsLock;
    int myMultiplier;
    bool myFullscreen;

    size_t myDragAndDropSlot;
    size_t myDragAndDropDrive;

    bool myScrollLockFullSpeed;

    common2::Speed mySpeed;

    std::shared_ptr<SDL_Window> myWindow;

    CConfigNeedingRestart myHardwareConfig;
  };

}
