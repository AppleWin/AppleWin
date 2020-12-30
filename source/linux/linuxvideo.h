#pragma once

#include "Video.h"

class LinuxVideo : public Video
{
public:
  virtual void Initialize();
  virtual void Destroy();

  virtual void VideoPresentScreen();
  virtual void ChooseMonochromeColor();
  virtual void Benchmark();
  virtual void DisplayLogo();
};
