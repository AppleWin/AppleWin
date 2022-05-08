#include "StdAfx.h"
#include "linux/linuxframe.h"
#include "linux/context.h"
#include "linux/network/slirp2.h"
#include "Tfe/PCapBackend.h"
#include "Interface.h"
#include "Log.h"
#include "Core.h"

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

bool LinuxFrame::GetBestDisplayResolutionForFullScreen(UINT& /* bestWidth */, UINT& /* bestHeight */, UINT /* userSpecifiedWidth */, UINT /* userSpecifiedHeight */)
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

void LinuxFrame::ResizeWindow()
{
}

void LinuxFrame::Initialize(bool resetVideoState)
{
  Video & video = GetVideo();

  const size_t numberOfPixels = video.GetFrameBufferWidth() * video.GetFrameBufferHeight();

  static_assert(sizeof(bgra_t) == 4, "Invalid size of bgra_t");
  const size_t numberOfBytes = sizeof(bgra_t) * numberOfPixels;

  myFramebuffer.resize(numberOfBytes);
  video.Initialize(myFramebuffer.data(), resetVideoState);
  LogFileTimeUntilFirstKeyReadReset();
}

void LinuxFrame::Destroy()
{
  myFramebuffer.clear();
  GetVideo().Destroy(); // this resets the Video's FrameBuffer pointer
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

void LinuxFrame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
{
  LogFileOutput("LoadBitmap: could not load resource %s\n", lpBitmapName);
  memset(lpvBits, 0, cb);
}

void LinuxFrame::Begin()
{
  InitialiseEmulator();
  Initialize(true);
}

void LinuxFrame::End()
{
  Destroy();
  DestroyEmulator();
}

void LinuxFrame::Restart()
{
  End();
  Begin();
}

std::shared_ptr<NetworkBackend> LinuxFrame::CreateNetworkBackend(const std::string & interfaceName)
{
#ifdef U2_USE_SLIRP
  return std::make_shared<SlirpBackend>();
#else
  return std::make_shared<PCapBackend>(interfaceName);
#endif
}

int MessageBox(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
  return GetFrame().FrameMessageBox(lpText, lpCaption, uType);
}
