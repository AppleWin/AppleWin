#include "linux/videobuffer.h"

#include "StdAfx.h"
#include "Video.h"
#include "NTSC.h"

void VideoBufferInitialize()
{
  static_assert(sizeof(bgra_t) == 4, "Invalid size of bgra_t");
  VideoResetState();

  const int numberOfPixels = GetFrameBufferWidth() * GetFrameBufferHeight();
  g_pFramebufferbits = static_cast<uint8_t *>(calloc(sizeof(bgra_t), numberOfPixels));
  NTSC_VideoInit(g_pFramebufferbits);
}

void VideoBufferDestroy()
{
  free(g_pFramebufferbits);
  g_pFramebufferbits = nullptr;
  NTSC_Destroy();
}
