#include "StdAfx.h"
#include "linux/linuxframe.h"
#include "Interface.h"

void LinuxFrame::FrameDrawDiskLEDS()
{
}

void LinuxFrame::FrameDrawDiskStatus()
{
}

void LinuxFrame::FrameRefreshStatus(int /* drawflags */)
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
  myFramebuffer.resize(numberOfBytes);
  video.Initialize(myFramebuffer.data());
}

void LinuxFrame::Destroy()
{
  myFramebuffer.clear();
  GetVideo().Destroy(); // this resets the Video's FrameBuffer pointer
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

void LinuxFrame::ApplyVideoModeChange()
{
  // this is similar to Win32Frame::ApplyVideoModeChange
  // but it does not refresh the screen
  // TODO see if the screen should refresh right now
  Video & video = GetVideo();

  video.VideoReinitialize(false);
  video.Config_Save_Video();

  FrameRefreshStatus(DRAW_TITLE);
}

void LinuxFrame::CycleVideoType()
{
  Video & video = GetVideo();
  video.IncVideoType();

  ApplyVideoModeChange();
}

void LinuxFrame::Cycle50ScanLines()
{
  Video & video = GetVideo();

  VideoStyle_e videoStyle = video.GetVideoStyle();
  videoStyle = VideoStyle_e(videoStyle ^ VS_HALF_SCANLINES);

  video.SetVideoStyle(videoStyle);

  ApplyVideoModeChange();
}
