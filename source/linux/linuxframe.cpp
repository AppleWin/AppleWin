#include "StdAfx.h"
#include "linux/linuxframe.h"

#include "Video.h"
#include "Windows/WinVideo.h"
#include "NTSC.h"

void LinuxFrame::FrameDrawDiskLEDS(HDC hdc)
{
}

void LinuxFrame::FrameDrawDiskStatus(HDC hdc)
{
}

void LinuxFrame::FrameRefreshStatus(int, bool /* bUpdateDiskStatus */)
{
}

void LinuxFrame::FrameUpdateApple2Type()
{
}

void LinuxFrame::FrameSetCursorPosByMousePos()
{
}

void LinuxFrame::VideoRedrawScreen()
{
  // NB. Can't rely on g_uVideoMode being non-zero (ie. so it can double up as a flag) since 'GR,PAGE1,non-mixed' mode == 0x00.
  VideoRefreshScreen(g_uVideoMode, true);
}

void LinuxFrame::SetFullScreenShowSubunitStatus(bool /* bShow */)
{
}

bool LinuxFrame::GetBestDisplayResolutionForFullScreen(UINT& /* bestWidth */, UINT& /* bestHeight */ , UINT /* userSpecifiedHeight */)
{
  return false;
}

int LinuxFrame::SetViewportScale(int nNewScale, bool /* bForce */)
{
  return nNewScale;
}

void LinuxFrame::SetAltEnterToggleFullScreen(bool /* mode */)
{
}

void LinuxFrame::SetLoadedSaveStateFlag(const bool /* bFlag */)
{
}
