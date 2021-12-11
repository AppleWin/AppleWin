#pragma once

#include "FrameBase.h"
#include <vector>

class LinuxFrame : public FrameBase
{
public:

  void Initialize(bool resetVideoState) override;
  void Destroy() override;

  void FrameDrawDiskLEDS() override;
  void FrameDrawDiskStatus() override;
  void FrameRefreshStatus(int drawflags) override;
  void FrameUpdateApple2Type() override;
  void FrameSetCursorPosByMousePos() override;
  void ResizeWindow() override;

  void SetFullScreenShowSubunitStatus(bool bShow) override;
  bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedWidth = 0, UINT userSpecifiedHeight = 0) override;
  int SetViewportScale(int nNewScale, bool bForce = false) override;
  void SetAltEnterToggleFullScreen(bool mode) override;

  void SetLoadedSaveStateFlag(const bool bFlag) override;

  void Restart() override; // calls End() - Begin()
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

  void CycleVideoType();
  void Cycle50ScanLines();

  void ApplyVideoModeChange();

  // these are wrappers around Initialize / Destroy that take care of initialising the emulator components
  // FrameBase::Initialize and ::Destroy only deal with the video part of the Frame, not the emulator
  // in AppleWin this happens in AppleWin.cpp, but it is useful to share it
  virtual void Begin();
  virtual void End();

protected:
  std::vector<uint8_t> myFramebuffer;
};

int MessageBox(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
