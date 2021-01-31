#pragma once

#include "frontends/common2/commonframe.h"

#include <memory>
#include <vector>

class RetroFrame : public CommonFrame
{
public:
  RetroFrame();

  void VideoPresentScreen() override;
  void FrameRefreshStatus(int drawflags) override;
  void Initialize() override;
  void Destroy() override;
  int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
  void GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits) override;

private:
  std::vector<uint8_t> myVideoBuffer;

  size_t myPitch;
  size_t myOffset;
  size_t myHeight;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;
  uint8_t* myFrameBuffer;
};
