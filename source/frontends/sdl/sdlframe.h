#pragma once

#include "Common.h"
#include "Configuration/Config.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/speed.h"
#include "frontends/common2/programoptions.h"
#include "linux/network/portfwds.h"
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
    std::shared_ptr<NetworkBackend> CreateNetworkBackend(const std::string & interfaceName) override;

    void ProcessEvents(bool &quit);

    void ExecuteOneFrame(const uint64_t microseconds);
    void ChangeMode(const AppMode_e mode);
    void SingleStep();
    void ResetHardware();
    bool HardwareChanged() const;

    void FrameResetMachineState();
    virtual void ResetSpeed();

    void LoadSnapshot() override;

    const std::shared_ptr<SDL_Window> & GetWindow() const;

    void getDragDropSlotAndDrive(size_t & slot, size_t & drive) const;
    void setDragDropSlotAndDrive(const size_t slot, const size_t drive);

    bool & getPreserveAspectRatio();

    const common2::Speed & getSpeed() const;

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

    void ExecuteInRunningMode(const uint64_t microseconds);
    void ExecuteInDebugMode(const uint64_t microseconds);
    void Execute(const DWORD uCycles);

    void SetFullSpeed(const bool value);
    bool CanDoFullSpeed();

    common2::Geometry getGeometryOrDefault(const std::optional<common2::Geometry> & geometry) const;

    static double GetRelativePosition(const int value, const int width);

    int myTargetGLSwap;
    bool myPreserveAspectRatio;
    bool myForceCapsLock;
    int myMultiplier;
    bool myFullscreen;

    size_t myDragAndDropSlot;
    size_t myDragAndDropDrive;

    bool myScrollLockFullSpeed;

    common2::Speed mySpeed;

    std::vector<PortFwd> myPortFwds;

    std::shared_ptr<SDL_Window> myWindow;

    CConfigNeedingRestart myHardwareConfig;
  };

}
