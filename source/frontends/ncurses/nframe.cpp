#include "StdAfx.h"
#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/colors.h"
#include "frontends/ncurses/asciiart.h"
#include "frontends/ncurses/evdevpaddle.h"
#include "Interface.h"
#include "Memory.h"
#include "Log.h"
#include "Core.h"
#include "CardManager.h"
#include "Disk.h"

#include <signal.h>
#include <locale.h>
#include <stdlib.h>

namespace na2
{

  struct NCurses
  {
    NCurses()
    {
      setlocale(LC_ALL, "");
      initscr();

      curs_set(0);

      noecho();
      cbreak();
      set_escdelay(0);

      // make sure this happens when ncurses is indeed initialised
      colors.reset(new GraphicsColors(20, 20, 32));
    }
    ~NCurses()
    {
      endwin();
      colors.reset();
    }
    std::shared_ptr<GraphicsColors> colors;
  };

  NFrame::NFrame(const std::shared_ptr<EvDevPaddle> & paddle)
    : myPaddle(paddle)
    , myRows(-1)
    , myColumns(-1)
  {
    // only initialise if actually used
    // so we can run headless
  }

  void NFrame::Initialize(bool resetVideoState)
  {
    CommonFrame::Initialize(resetVideoState);
    myTextFlashCounter = 0;
    myTextFlashState = 0;
    myAsciiArt.reset(new ASCIIArt());
  }

  void NFrame::Destroy()
  {
    CommonFrame::Destroy();
    myTextFlashCounter = 0;
    myTextFlashState = 0;
    myFrame.reset();
    myStatus.reset();
    myAsciiArt.reset();

    myNCurses.reset();
  }

  void NFrame::ProcessEvDev()
  {
    myPaddle->poll();
  }

  void NFrame::ChangeColumns(const int x)
  {
    myAsciiArt->changeColumns(x);
  }

  void NFrame::ChangeRows(const int x)
  {
    myAsciiArt->changeRows(x);
  }

  void NFrame::Init(int rows, int columns)
  {
    if (myRows != rows || myColumns != columns)
    {
      InitialiseNCurses();
      if (columns < myColumns || rows < myRows)
      {
        werase(myStatus.get());
        wrefresh(myStatus.get());
        werase(myFrame.get());
        wrefresh(myFrame.get());
      }

      myRows = rows;
      myColumns = columns;

      const int width = 1 + myColumns + 1;
      const int left = (COLS - width) / 2;

      myFrame.reset(newwin(1 + myRows + 1, width, 0, left), delwin);
      box(myFrame.get(), 0 , 0);
      wtimeout(myFrame.get(), 0);
      keypad(myFrame.get(), true);
      wrefresh(myFrame.get());

      myStatus.reset(newwin(8, width, 1 + myRows + 1, left), delwin);
      FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
    }
  }

  WINDOW * NFrame::GetWindow()
  {
    return myFrame.get();
  }

  WINDOW * NFrame::GetStatus()
  {
    return myStatus.get();
  }

  void NFrame::InitialiseNCurses()
  {
    if (!myNCurses)
    {
      myNCurses.reset(new NCurses());
    }
  }

  void NFrame::VideoPresentScreen()
  {
    VideoUpdateFlash();

    Video & video = GetVideo();

    // see NTSC_SetVideoMode in NTSC.cpp
    // we shoudl really use g_nTextPage and g_nHiresPage
    const int displaypage2 = (video.VideoGetSWPAGE2() && !video.VideoGetSW80STORE()) ? 1 : 0;

    myHiresBank1 = MemGetAuxPtr (0x2000 << displaypage2);
    myHiresBank0 = MemGetMainPtr(0x2000 << displaypage2);
    myTextBank1  = MemGetAuxPtr (0x400  << displaypage2);
    myTextBank0  = MemGetMainPtr(0x400  << displaypage2);

    typedef bool (NFrame::* VideoUpdateFuncPtr_t)(Video &, int, int, int, int, int);

    VideoUpdateFuncPtr_t update = video.VideoGetSWTEXT()
      ? video.VideoGetSW80COL()
      ? &NFrame::Update80ColCell
      : &NFrame::Update40ColCell
      : video.VideoGetSWHIRES()
      ? (video.VideoGetSWDHIRES() && video.VideoGetSW80COL())
      ? &NFrame::UpdateDHiResCell
      : &NFrame::UpdateHiResCell
      : (video.VideoGetSWDHIRES() && video.VideoGetSW80COL())
      ? &NFrame::UpdateDLoResCell
      : &NFrame::UpdateLoResCell;

    int  y        = 0;
    int  ypixel   = 0;
    while (y < 20) {
      int offset = ((y & 7) << 7) + ((y >> 3) * 40);
      int x      = 0;
      int xpixel = 0;
      while (x < 40) {
        (this->*update)(video, x, y, xpixel, ypixel, offset + x);
        ++x;
        xpixel += 14;
      }
      ++y;
      ypixel += 16;
    }

    if (video.VideoGetSWMIXED())
      update = video.VideoGetSW80COL() ? &NFrame::Update80ColCell
        : &NFrame::Update40ColCell;

    while (y < 24) {
      int offset = ((y & 7) << 7) + ((y >> 3) * 40);
      int x      = 0;
      int xpixel = 0;
      while (x < 40) {
        (this->*update)(video, x, y, xpixel, ypixel, offset + x);
        ++x;
        xpixel += 14;
      }
      ++y;
      ypixel += 16;
    }

    wrefresh(myFrame.get());
  }

  void NFrame::FrameRefreshStatus(int /* drawflags */)
  {
    werase(myStatus.get());
    box(myStatus.get(), 0 , 0);

    int row = 0;

    CardManager& cardManager = GetCardMgr();
    if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
    {
      Disk2InterfaceCard& disk2 = dynamic_cast<Disk2InterfaceCard&>(cardManager.GetRef(SLOT6));
      const size_t maximumWidth = myColumns - 6;  // 6 is the width of "S6D1: "
      for (UINT i = DRIVE_1; i <= DRIVE_2; ++i)
      {
        const std::string name = disk2.GetBaseName(i).substr(0, maximumWidth);
        mvwprintw(myStatus.get(), ++row, 1, "S6D%d: %s", 1 + i, name.c_str());
      }
    }
    else
    {
      row += DRIVE_2 - DRIVE_1 + 1;
    }

    ++row;

    mvwprintw(myStatus.get(), ++row, 1, "F2: ResetMachine / Shift-F2: CtrlReset");
    mvwprintw(myStatus.get(), ++row, 1, "F3: Exit         / F5: Swap");
    mvwprintw(myStatus.get(), ++row, 1, "F11: Load State  / F12: Save State");
    wrefresh(myStatus.get());
  }

  void NFrame::VideoUpdateFlash()
  {
    ++myTextFlashCounter;

    if (myTextFlashCounter == 16) // Flash rate = 0.5 * 60 / 16 Hz (as we need 2 changes for a period)
    {
      myTextFlashCounter = 0;
      myTextFlashState = !myTextFlashState;
    }
  }

  chtype NFrame::MapCharacter(Video & video, BYTE ch)
  {
    const char low = ch & 0x7f;
    const char high = ch & 0x80;

    chtype result = low;

    const int code = low >> 5;
    switch (code)
    {
    case 0:           // 00 - 1F
      result += 0x40; // UPPERCASE
      break;
    case 1:           // 20 - 3F
      // SPECIAL CHARACTER
      break;
    case 2:           // 40 - 5F
      // UPPERCASE
      break;
    case 3:           // 60 - 7F
      // LOWERCASE
      if (high == 0 && !video.VideoGetSWAltCharSet())
      {
        result -= 0x40;
      }
      break;
    }

    if (result == 0x7f)
    {
      result = ACS_CKBOARD;
    }

    if (!high)
    {
      if (!video.VideoGetSWAltCharSet() && (low >= 0x40))
      {
        // result |= A_BLINK; // does not work on my terminal
        if (myTextFlashState)
        {
          result |= A_REVERSE;
        }
      }
      else
      {
        result |= A_REVERSE;
      }
    }

    return result;
  }

  bool NFrame::Update40ColCell(Video & video, int x, int y, int xpixel, int ypixel, int offset)
  {
    Init(24, 40);
    myAsciiArt->init(1, 1);

    BYTE ch = *(myTextBank0+offset);

    const chtype ch2 = MapCharacter(video, ch);
    mvwaddch(myFrame.get(), 1 + y, 1 + x, ch2);

    return true;
  }

  bool NFrame::Update80ColCell(Video & video, int x, int y, int xpixel, int ypixel, int offset)
  {
    Init(24, 80);
    myAsciiArt->init(1, 2);

    BYTE ch1 = *(myTextBank1+offset);
    BYTE ch2 = *(myTextBank0+offset);

    WINDOW * win = myFrame.get();

    const chtype ch12 = MapCharacter(video, ch1);
    mvwaddch(win, 1 + y, 1 + 2 * x, ch12);

    const chtype ch22 = MapCharacter(video, ch2);
    mvwaddch(win, 1 + y, 1 + 2 * x + 1, ch22);

    return true;
  }

  bool NFrame::UpdateLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    BYTE val = *(myTextBank0+offset);

    const int pair = myNCurses->colors->getPair(val);

    WINDOW * win = myFrame.get();

    wcolor_set(win, pair, NULL);
    if (myColumns == 40)
    {
      mvwaddstr(win, 1 + y, 1 + x, "\u2580");
    }
    else
    {
      mvwaddstr(win, 1 + y, 1 + 2 * x, "\u2580\u2580");
    }
    wcolor_set(win, 0, NULL);

    return true;
  }

  bool NFrame::UpdateDLoResCell(Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

  bool NFrame::UpdateHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    const BYTE * base = myHiresBank0 + offset;

    const ASCIIArt::array_char_t & chs = myAsciiArt->getCharacters(base);

    const auto shape = chs.shape();
    const size_t rows = shape[0];
    const size_t cols = shape[1];

    Init(24 * rows, 40 * cols);
    WINDOW * win = myFrame.get();

    const GraphicsColors & colors = *myNCurses->colors;

    for (size_t i = 0; i < rows; ++i)
    {
      for (size_t j = 0; j < cols; ++j)
      {
        const int pair = colors.getGrey(chs[i][j].foreground, chs[i][j].background);

        wcolor_set(win, pair, NULL);
        mvwaddstr(win, 1 + rows * y + i, 1 + cols * x + j, chs[i][j].c);
      }
    }
    wcolor_set(win, 0, NULL);

    return true;
  }

  bool NFrame::UpdateDHiResCell(Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

  int NFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
  {
    LogFileOutput("MessageBox:\n%s\n%s\n\n", lpCaption, lpText);
    return IDOK;
  }

}

void SingleStep(bool /* bReinit */)
{

}
