#include "StdAfx.h"
#include "linux/linuxframe.h"
#include "linux/bitmap.h"
#include "linux/context.h"
#include "linux/network/slirp2.h"
#include "linux/version.h"
#include "Interface.h"
#include "Log.h"
#include "Core.h"
#include "SaveState.h"

#ifndef U2_USE_SLIRP
#include "Tfe/PCapBackend.h"
#endif

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

bool LinuxFrame::GetBestDisplayResolutionForFullScreen(
    UINT & /* bestWidth */, UINT & /* bestHeight */, UINT /* userSpecifiedWidth */, UINT /* userSpecifiedHeight */)
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

void LinuxFrame::SetWindowedModeShowDiskiiStatus(bool /* bShow */)
{
}

LinuxFrame::LinuxFrame(const bool autoBoot)
    : myAutoBoot(autoBoot)
{
    const std::array<int, 4> version = getVersionNumbers();
    SetAppleWinVersion(version[0], version[1], version[2], version[3]);
}

void LinuxFrame::Initialize(bool resetVideoState)
{
    Video &video = GetVideo();

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
    Video &video = GetVideo();

    video.VideoReinitialize(false);
    video.Config_Save_Video();

    FrameRefreshStatus(DRAW_TITLE);
}

void LinuxFrame::CycleVideoType()
{
    Video &video = GetVideo();
    video.IncVideoType();

    ApplyVideoModeChange();
}

void LinuxFrame::Cycle50ScanLines()
{
    Video &video = GetVideo();

    VideoStyle_e videoStyle = video.GetVideoStyle();
    videoStyle = VideoStyle_e(videoStyle ^ VS_HALF_SCANLINES);

    video.SetVideoStyle(videoStyle);

    ApplyVideoModeChange();
}

BYTE *LinuxFrame::GetResource(WORD id, LPCSTR lpType, uint32_t expectedSize)
{
    const auto resource = GetResourceData(id);

    if (resource.second != expectedSize)
    {
        throw std::runtime_error(
            "Invalid size for resource " + std::to_string(id) + ": " + std::to_string(resource.second) +
            " != " + std::to_string(expectedSize));
    }

    auto data = const_cast<unsigned char *>(resource.first);
    return reinterpret_cast<BYTE *>(data);
}

void LinuxFrame::GetBitmap(WORD id, LONG cb, LPVOID lpvBits)
{
    const auto resource = GetResourceData(id);
    if (!GetBitmapFromResource(resource, cb, lpvBits))
    {
        throw std::runtime_error("Cannot process bitmap resource " + std::to_string(id));
    }
}

void LinuxFrame::Begin()
{
    InitialiseEmulator(myAutoBoot ? MODE_RUNNING : MODE_PAUSED);
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

void LinuxFrame::LoadSnapshot()
{
    Snapshot_LoadState();
}

std::shared_ptr<NetworkBackend> LinuxFrame::CreateNetworkBackend(const std::string &interfaceName)
{
#ifdef U2_USE_SLIRP
    return std::make_shared<SlirpBackend>();
#else
    return std::make_shared<PCapBackend>(interfaceName);
#endif
}

#ifndef _WIN32
int MessageBox(HWND, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    return GetFrame().FrameMessageBox(lpText, lpCaption, uType);
}
#endif
