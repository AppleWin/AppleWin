#include "StdAfx.h"

#include "frontends/libretro/retroframe.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/environment.h"
#include "frontends/common2/utils.h"

#include "Interface.h"
#include "Core.h"
#include "Utilities.h"

namespace ra2
{

    RetroFrame::RetroFrame(const common2::EmulatorOptions &options, const size_t linePeriod)
        : common2::GNUFrame(options)
        , myLinePeriod(linePeriod)
    {
    }

    void RetroFrame::FrameRefreshStatus(int drawflags)
    {
        if (drawflags & DRAW_TITLE)
        {
            GetAppleWindowTitle();
            display_message(g_pAppTitle.c_str());
        }
    }

    void RetroFrame::VideoPresentScreen()
    {
        // this should not be necessary
        // either libretro handles it
        // or we should change AW
        // but for now, there is no alternative
        const size_t offset = myLinePeriod - 1;
        for (size_t row = 0; row < myHeight; ++row)
        {
            // row is in "retro" space, so we need to adjust it
            // to the original frame buffer space
            const uint8_t *src = myFrameBuffer + (row * myLinePeriod + offset) * myPitch;
            uint8_t *dst = myVideoBuffer.data() + (myHeight - row - 1) * myPitch;
            memcpy(dst, src, myPitch);
        }

        video_cb(myVideoBuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
    }

    void RetroFrame::Initialize(bool resetVideoState)
    {
        CommonFrame::Initialize(resetVideoState);
        FrameRefreshStatus(DRAW_TITLE);

        Video &video = GetVideo();

        // This initialises the libretro video buffer, so we apply LinePeriod everywhere.
        myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
        myBorderlessHeight = video.GetFrameBufferBorderlessHeight() / myLinePeriod;
        const size_t borderWidth = video.GetFrameBufferBorderWidth();
        const size_t borderHeight = video.GetFrameBufferBorderHeight() / myLinePeriod;
        const size_t width = video.GetFrameBufferWidth();
        myHeight = video.GetFrameBufferHeight() / myLinePeriod;

        myFrameBuffer = video.GetFrameBuffer();

        myPitch = width * sizeof(bgra_t);
        myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

        const size_t size = myHeight * myPitch;
        myVideoBuffer.resize(size);
    }

    void RetroFrame::Destroy()
    {
        CommonFrame::Destroy();
        myFrameBuffer = nullptr;
        myVideoBuffer.clear();
    }

    int RetroFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
    {
        log_cb(RETRO_LOG_INFO, "RA2: %s: %s - %s\n", __FUNCTION__, lpCaption, lpText);
        return IDOK;
    }

    void RetroFrame::SetFullSpeed(const bool /* value */)
    {
        // do nothing
    }

    bool RetroFrame::CanDoFullSpeed()
    {
        // Let libretro deal with it.
        return false;
    }

    void RetroFrame::Begin()
    {
        const common2::RestoreCurrentDirectory restoreChDir;
        common2::GNUFrame::Begin();
    }

    std::shared_ptr<SoundBuffer> RetroFrame::CreateSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName)
    {
        const auto buffer = ra2::iCreateDirectSoundBuffer(dwBufferSize, nSampleRate, nChannels, pszVoiceName);
        return buffer;
    }

    size_t RetroFrame::GetFrameBufferLinePeriod() const
    {
        return myLinePeriod;
    }

} // namespace ra2
