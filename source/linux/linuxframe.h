#pragma once

#include "FrameBase.h"
#include <vector>

class LinuxFrame : public FrameBase
{
public:

  virtual void Initialize();
  virtual void Destroy();

  virtual void FrameDrawDiskLEDS();
  virtual void FrameDrawDiskStatus();
  virtual void FrameRefreshStatus(int drawflags);
  virtual void FrameUpdateApple2Type();
  virtual void FrameSetCursorPosByMousePos();

  virtual void SetFullScreenShowSubunitStatus(bool bShow);
  virtual bool GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight = 0);
  virtual int SetViewportScale(int nNewScale, bool bForce = false);
  virtual void SetAltEnterToggleFullScreen(bool mode);

  virtual void SetLoadedSaveStateFlag(const bool bFlag);

  virtual void VideoPresentScreen();
  virtual void ChooseMonochromeColor();
  virtual void Benchmark();
  virtual void DisplayLogo();

private:
  std::vector<uint8_t> myFramebufferbits;
};
