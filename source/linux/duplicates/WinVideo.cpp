#include "StdAfx.h"

#include "Video.h"
#include "NTSC.h"

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
