#include "StdAfx.h"
#include "linux/linuxframe.h"

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
