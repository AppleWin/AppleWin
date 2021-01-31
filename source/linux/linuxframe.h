#pragma once

#include "FrameBase.h"
#include <vector>

class LinuxFrame : public FrameBase
{
public:

  void Initialize() override;
  void Destroy() override;

  void FrameDrawDiskLEDS() override;
  void FrameDrawDiskStatus() override;
  void FrameRefreshStatus(int drawflags) override;
  void FrameUpdateApple2Type() override;
  void FrameSetCursorPosByMousePos() override;

  void SetFullScreenShowSubunitStatus(bool bShow) override;
  bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0) override;
  int SetViewportScale(int nNewScale, bool bForce = false) override;
  void SetAltEnterToggleFullScreen(bool mode) override;

  void SetLoadedSaveStateFlag(const bool bFlag) override;

  void Restart() override;
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

  void CycleVideoType();
  void Cycle50ScanLines();

  void ApplyVideoModeChange();

protected:
  std::vector<uint8_t> myFramebuffer;
};

int MessageBox(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
