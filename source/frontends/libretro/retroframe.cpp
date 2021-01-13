#include "StdAfx.h"
#include "frontends/libretro/retroframe.h"
#include "frontends/libretro/environment.h"

#include "Interface.h"
#include "Core.h"
#include "Utilities.h"

RetroFrame::RetroFrame()
{
}

void RetroFrame::FrameRefreshStatus(int drawflags)
{
  if (drawflags & DRAW_TITLE)
  {
    GetAppleWindowTitle();
    display_message(g_pAppTitle.c_str());
  }
}

void RetroFrame::VideoPresentScreen()
{
  // this should not be necessary
  // either libretro handles it
  // or we should change AW
  // but for now, there is no alternative
  for (size_t row = 0; row < myHeight; ++row)
  {
    const uint8_t * src = myFrameBuffer + row * myPitch;
    uint8_t * dst = myVideoBuffer.data() + (myHeight - row - 1) * myPitch;
    memcpy(dst, src, myPitch);
  }

  video_cb(myVideoBuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
}

void RetroFrame::Initialize()
{
  LinuxFrame::Initialize();
  FrameRefreshStatus(DRAW_TITLE);

  Video & video = GetVideo();

  myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
  myBorderlessHeight = video.GetFrameBufferBorderlessHeight();
  const size_t borderWidth = video.GetFrameBufferBorderWidth();
  const size_t borderHeight = video.GetFrameBufferBorderHeight();
  const size_t width = video.GetFrameBufferWidth();
  myHeight = video.GetFrameBufferHeight();

  myFrameBuffer = video.GetFrameBuffer();

  myPitch = width * sizeof(bgra_t);
  myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

  const size_t size = myHeight * myPitch;
  myVideoBuffer.resize(size);
}

void RetroFrame::Destroy()
{
  LinuxFrame::Destroy();
  myFrameBuffer = nullptr;
  myVideoBuffer.clear();
}
