#include "StdAfx.h"

#include "linux/linuxvideo.h"
#include "NTSC.h"

void LinuxVideo::Initialize()
{
  static_assert(sizeof(bgra_t) == 4, "Invalid size of bgra_t");
  VideoResetState();

  const int numberOfPixels = GetFrameBufferWidth() * GetFrameBufferHeight();
  g_pFramebufferbits = static_cast<uint8_t *>(calloc(sizeof(bgra_t), numberOfPixels));
  NTSC_VideoInit(g_pFramebufferbits);
}

void LinuxVideo::Destroy()
{
  free(g_pFramebufferbits);
  g_pFramebufferbits = nullptr;
  NTSC_Destroy();
}

void LinuxVideo::VideoPresentScreen()
{
  // TODO we should really implement this
}

void LinuxVideo::ChooseMonochromeColor()
{
}

void LinuxVideo::Benchmark()
{
}

void LinuxVideo::DisplayLogo()
{
}
