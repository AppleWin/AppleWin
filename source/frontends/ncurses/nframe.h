#pragma once

#include "frontends/common2/gnuframe.h"
#include "frontends/ncurses/ncurses_safe.h"

#include <memory>
#include <string>

namespace na2
{

    class ASCIIArt;
    class EvDevPaddle;
    struct NCurses;

    class NFrame : public common2::GNUFrame
    {
    public:
        NFrame(const common2::EmulatorOptions &options, const std::shared_ptr<EvDevPaddle> &paddle);

        WINDOW *GetWindow();

        void Initialize(bool resetVideoState) override;
        void Destroy() override;
        void VideoPresentScreen() override;
        int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
        void FrameRefreshStatus(int drawflags) override;

        std::shared_ptr<SoundBuffer> CreateSoundBuffer(
            uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName) override;

        void ProcessEvDev();

        void ChangeColumns(const int x);
        void ChangeRows(const int x);
        void ToggleFullscreen();

        void Init(int rows, int columns);
        void ReInit();

    private:
        const std::shared_ptr<EvDevPaddle> myPaddle;

        bool myFullscreen;

        int myExtraRows = 0;
        int myExtraCols = 0;

        int myRows = -1;
        int myColumns = -1;
        int myTextFlashCounter = 0;
        bool myTextFlashState = false;

        std::shared_ptr<WINDOW> myFrame;
        std::shared_ptr<WINDOW> myStatus;
        std::shared_ptr<ASCIIArt> myAsciiArt;
        std::shared_ptr<NCurses> myNCurses;

        LPBYTE myTextBank1; // Aux
        LPBYTE myTextBank0; // Main
        LPBYTE myHiresBank1;
        LPBYTE myHiresBank0;

        void VideoUpdateFlash();

        chtype MapCharacter(Video &video, BYTE ch);

        bool Update40ColCell(Video &video, int x, int y, int xpixel, int ypixel, int offset);
        bool Update80ColCell(Video &video, int x, int y, int xpixel, int ypixel, int offset);
        bool UpdateLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
        bool UpdateDLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
        bool UpdateHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
        bool UpdateDHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);

        void InitialiseNCurses();
        void ForceInit(int rows, int columns, bool fullScreen);
    };

} // namespace na2
