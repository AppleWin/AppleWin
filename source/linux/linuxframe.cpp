#include "StdAfx.h"
#include "linux/linuxframe.h"
#include "Interface.h"

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

void LinuxFrame::Initialize()
{
  static_assert(sizeof(bgra_t) == 4, "Invalid size of bgra_t");
  Video & video = GetVideo();

  const size_t numberOfPixels = video.GetFrameBufferWidth() * video.GetFrameBufferHeight();
  const size_t numberOfBytes = sizeof(bgra_t) * numberOfPixels;
  myFramebufferbits.resize(numberOfBytes);
  video.Initialize(myFramebufferbits.data());
}

void LinuxFrame::Destroy()
{
  myFramebufferbits.clear();
  GetVideo().Destroy(); // this resets the Video's FrameBuffer pointer
}

void LinuxFrame::VideoPresentScreen()
{
}

void LinuxFrame::ChooseMonochromeColor()
{
}

void LinuxFrame::Benchmark()
{
}

void LinuxFrame::DisplayLogo()
{
}
