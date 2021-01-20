#pragma once

#include "frontends/common2/commonframe.h"

#include <memory>
#include <vector>

class RetroFrame : public CommonFrame
{
public:
  RetroFrame();

  virtual void VideoPresentScreen();
  virtual void FrameRefreshStatus(int drawflags);
  virtual void Initialize();
  virtual void Destroy();
  virtual int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType);
  virtual void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits);

private:
  std::vector<uint8_t> myVideoBuffer;

  size_t myPitch;
  size_t myOffset;
  size_t myHeight;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;
  uint8_t* myFrameBuffer;
};
