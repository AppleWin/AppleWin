#pragma once

#include "Video.h"

class LinuxVideo : public Video
{
public:
  virtual void Initialize();
  virtual void Destroy();

  virtual void VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit = false);
  virtual void VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame);
  virtual void VideoRefreshScreen(uint32_t uRedrawWholeScreenVideoMode = 0, bool bRedrawWholeScreen = false);
  virtual void Video_RedrawAndTakeScreenShot(const char* pScreenshotFilename);
  virtual void ChooseMonochromeColor();
  virtual void Benchmark();
  virtual void DisplayLogo();
};
