#include "linux/videobuffer.h"

#include "StdAfx.h"
#include "Video.h"
#include "Frame.h"
#include "NTSC.h"

void VideoBufferInitialize()
{
  VideoResetState();
  g_pFramebufferbits = static_cast<uint8_t *>(calloc(sizeof(bgra_t), GetFrameBufferWidth() * GetFrameBufferHeight()));
  NTSC_VideoInit(g_pFramebufferbits);
}

void VideoBufferDestroy()
{
  free(g_pFramebufferbits);
  g_pFramebufferbits = nullptr;
  NTSC_Destroy();
}

void getScreenData(uint8_t * & data, int & width, int & height, int & sx, int & sy, int & sw, int & sh)
{
  data = g_pFramebufferbits;
  width = GetFrameBufferWidth();
  height = GetFrameBufferHeight();
  sx = GetFrameBufferBorderWidth();
  sy = GetFrameBufferBorderHeight();
  sw = GetFrameBufferBorderlessWidth();
  sh = GetFrameBufferBorderlessHeight();
}
