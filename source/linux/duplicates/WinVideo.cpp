#include "StdAfx.h"

#include "Video.h"
#include "NTSC.h"
#include "Windows/WinVideo.h"

void VideoRedrawScreen(void)
{
  // NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
  VideoRefreshScreen(g_uVideoMode, true);
}

void VideoRefreshScreen ( uint32_t uRedrawWholeScreenVideoMode /* =0*/, bool bRedrawWholeScreen /* =false*/ )
{
  if (bRedrawWholeScreen)
  {
    // uVideoModeForWholeScreen set if:
    // . MODE_DEBUG   : always
    // . MODE_RUNNING : called from VideoRedrawScreen(), eg. during full-speed
    if (bRedrawWholeScreen)
      NTSC_SetVideoMode(uRedrawWholeScreenVideoMode);
    NTSC_VideoRedrawWholeScreen();
  }
}
