#pragma once

#include "linux/linuxframe.h"

#include <memory>
#include <vector>

class RetroFrame : public LinuxFrame
{
public:
  RetroFrame();

  virtual void VideoPresentScreen();
  virtual void FrameRefreshStatus(int drawflags);
  virtual void Initialize();
  virtual void Destroy();
private:
  std::vector<uint8_t> myVideoBuffer;

  size_t myPitch;
  size_t myOffset;
  size_t myHeight;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;
  uint8_t* myFrameBuffer;
};
