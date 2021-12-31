#pragma once

#include "frontends/common2/commonframe.h"

#include <memory>
#include <string>
#include <ncurses.h>

namespace na2
{

  class ASCIIArt;
  class EvDevPaddle;
  struct NCurses;

  class NFrame : public common2::CommonFrame
  {
  public:
    NFrame(const std::shared_ptr<EvDevPaddle> & paddle);

    WINDOW * GetWindow();
    WINDOW * GetStatus();

    void Initialize(bool resetVideoState) override;
    void Destroy() override;
    void VideoPresentScreen() override;
    int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
    void FrameRefreshStatus(int drawflags);

    void ProcessEvDev();

    void ChangeColumns(const int x);
    void ChangeRows(const int x);

    void Init(int rows, int columns);

  private:

    const std::shared_ptr<EvDevPaddle> myPaddle;

    int myRows;
    int myColumns;
    int myTextFlashCounter;
    bool myTextFlashState;

    std::shared_ptr<WINDOW> myFrame;
    std::shared_ptr<WINDOW> myStatus;
    std::shared_ptr<ASCIIArt> myAsciiArt;
    std::shared_ptr<NCurses> myNCurses;

    LPBYTE        myTextBank1; // Aux
    LPBYTE        myTextBank0; // Main
    LPBYTE        myHiresBank1;
    LPBYTE        myHiresBank0;

    void VideoUpdateFlash();

    chtype MapCharacter(Video & video, BYTE ch);

    bool Update40ColCell(Video & video, int x, int y, int xpixel, int ypixel, int offset);
    bool Update80ColCell(Video & video, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset);

    void InitialiseNCurses();
  };

}
