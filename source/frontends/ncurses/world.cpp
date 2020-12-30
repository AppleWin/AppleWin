#include "frontends/ncurses/world.h"
#include "StdAfx.h"

#include <ncurses.h>
#include <signal.h>
#include <iostream>

#include "Common.h"
#include "CardManager.h"
#include "Disk.h"
#include "Video.h"
#include "CPU.h"
#include "Log.h"
#include "DiskImageHelper.h"
#include "Memory.h"
#include "Core.h"
#include "RGBMonitor.h"
#include "Interface.h"

#include "linux/interface.h"
#include "linux/paddle.h"
#include "linux/data.h"
#include "linux/keyboard.h"

#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/colors.h"
#include "frontends/ncurses/asciiart.h"
#include "frontends/ncurses/evdevpaddle.h"

namespace
{

  std::shared_ptr<Frame> frame;
  std::shared_ptr<ASCIIArt> asciiArt;
  std::shared_ptr<EvDevPaddle> paddle;

  int    g_nTrackDrive1  = -1;
  int    g_nTrackDrive2  = -1;
  int    g_nSectorDrive1 = -1;
  int    g_nSectorDrive2 = -1;
  TCHAR  g_sTrackDrive1 [8] = TEXT("??");
  TCHAR  g_sTrackDrive2 [8] = TEXT("??");
  TCHAR  g_sSectorDrive1[8] = TEXT("??");
  TCHAR  g_sSectorDrive2[8] = TEXT("??");
  Disk_Status_e g_eStatusDrive1 = DISK_STATUS_OFF;
  Disk_Status_e g_eStatusDrive2 = DISK_STATUS_OFF;

  LPBYTE        g_pTextBank1; // Aux
  LPBYTE        g_pTextBank0; // Main
  LPBYTE        g_pHiresBank1;
  LPBYTE        g_pHiresBank0;

  bool g_bTextFlashState = false;

  void sig_handler_pass(int signo)
  {
    // Ctrl-C
    // is there a race condition here?
    // is it a problem?
    addKeyToBuffer(0x03);
  }

  void sig_handler_exit(int signo)
  {
    g_stop = true;
  }

  chtype mapCharacter(Video & video, BYTE ch)
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
	if (g_bTextFlashState)
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

  void VideoUpdateFlash()
  {
    static UINT nTextFlashCnt = 0;

    ++nTextFlashCnt;

    if (nTextFlashCnt == 16) // Flash rate = 0.5 * 60 / 16 Hz (as we need 2 changes for a period)
    {
      nTextFlashCnt = 0;
      g_bTextFlashState = !g_bTextFlashState;
    }
  }

  typedef bool (*VideoUpdateFuncPtr_t)(Video &, int, int, int, int, int);

  bool NUpdate40ColCell (Video & video, int x, int y, int xpixel, int ypixel, int offset)
  {
    frame->init(24, 40);
    asciiArt->init(1, 1);

    BYTE ch = *(g_pTextBank0+offset);

    WINDOW * win = frame->getWindow();

    const chtype ch2 = mapCharacter(video, ch);
    mvwaddch(win, 1 + y, 1 + x, ch2);

    return true;
  }

  bool NUpdate80ColCell (Video & video, int x, int y, int xpixel, int ypixel, int offset)
  {
    frame->init(24, 80);
    asciiArt->init(1, 2);

    BYTE ch1 = *(g_pTextBank1+offset);
    BYTE ch2 = *(g_pTextBank0+offset);

    WINDOW * win = frame->getWindow();

    const chtype ch12 = mapCharacter(video, ch1);
    mvwaddch(win, 1 + y, 1 + 2 * x, ch12);

    const chtype ch22 = mapCharacter(video, ch2);
    mvwaddch(win, 1 + y, 1 + 2 * x + 1, ch22);

    return true;
  }

  bool NUpdateLoResCell (Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    BYTE val = *(g_pTextBank0+offset);

    const int pair = frame->getColors().getPair(val);

    WINDOW * win = frame->getWindow();

    wcolor_set(win, pair, NULL);
    if (frame->getColumns() == 40)
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

  bool NUpdateDLoResCell (Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

  bool NUpdateHiResCell (Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    const BYTE * base = g_pHiresBank0 + offset;

    const ASCIIArt::array_char_t & chs = asciiArt->getCharacters(base);

    const auto shape = chs.shape();
    const size_t rows = shape[0];
    const size_t cols = shape[1];

    frame->init(24 * rows, 40 * cols);
    WINDOW * win = frame->getWindow();

    const GraphicsColors & colors = frame->getColors();

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

  bool NUpdateDHiResCell (Video &, int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

}

double g_relativeSpeed = 1.0;
bool g_stop = false;

int MessageBox(HWND, const char * text, const char * caption, UINT)
{
  LogFileOutput("MessageBox:\n%s\n%s\n\n", caption, text);
  return IDOK;
}

void NVideoInitialize(const bool headless)
{
  frame.reset(new Frame());
  asciiArt.reset(new ASCIIArt());

  paddle.reset(new EvDevPaddle("/dev/input/by-id/usb-©Microsoft_Corporation_Controller_1BBE3DB-event-joystick"));

  Paddle::instance = paddle;

  if (headless)
  {
    signal(SIGINT, sig_handler_exit);
  }
  else
  {
    signal(SIGINT, sig_handler_pass);
    // pass Ctrl-C to the emulator
  }
}

void NVideoRedrawScreen()
{
  VideoUpdateFlash();

  Video & video = GetVideo();

  const int displaypage2 = video.VideoGetSWPAGE2() == 0 ? 0 : 1;

  g_pHiresBank1 = MemGetAuxPtr (0x2000 << displaypage2);
  g_pHiresBank0 = MemGetMainPtr(0x2000 << displaypage2);
  g_pTextBank1  = MemGetAuxPtr (0x400  << displaypage2);
  g_pTextBank0  = MemGetMainPtr(0x400  << displaypage2);

  VideoUpdateFuncPtr_t update = video.VideoGetSWTEXT()
    ? video.VideoGetSW80COL()
    ? NUpdate80ColCell
    : NUpdate40ColCell
    : video.VideoGetSWHIRES()
    ? (video.VideoGetSWDHIRES() && video.VideoGetSW80COL())
    ? NUpdateDHiResCell
    : NUpdateHiResCell
    : (video.VideoGetSWDHIRES() && video.VideoGetSW80COL())
    ? NUpdateDLoResCell
    : NUpdateLoResCell;

  int  y        = 0;
  int  ypixel   = 0;
  while (y < 20) {
    int offset = ((y & 7) << 7) + ((y >> 3) * 40);
    int x      = 0;
    int xpixel = 0;
    while (x < 40) {
      update(video, x, y, xpixel, ypixel, offset + x);
      ++x;
      xpixel += 14;
    }
    ++y;
    ypixel += 16;
  }

  if (video.VideoGetSWMIXED())
    update = video.VideoGetSW80COL() ? NUpdate80ColCell
      : NUpdate40ColCell;

  while (y < 24) {
    int offset = ((y & 7) << 7) + ((y >> 3) * 40);
    int x      = 0;
    int xpixel = 0;
    while (x < 40) {
      update(video, x, y, xpixel, ypixel, offset + x);
      ++x;
      xpixel += 14;
    }
    ++y;
    ypixel += 16;
  }

  wrefresh(frame->getWindow());
}

int ProcessKeyboard()
{
  WINDOW * window = frame->getWindow();
  if (!window)
  {
    return ERR;
  }

  const int inch = wgetch(window);

  int ch = ERR;

  switch (inch)
  {
  case ERR:
    break;
  case '\n':
    ch = 0x0d; // ENTER
    break;
  case KEY_BACKSPACE:
  case KEY_LEFT:
    ch = 0x08;
    break;
  case KEY_RIGHT:
    ch = 0x15;
    break;
  case KEY_UP:
    ch = 0x0b;
    break;
  case KEY_DOWN:
    ch = 0x0a;
    break;
  case 0x14a: // DEL
    ch = 0x7f;
    break;
  case 543 ... 546: // Various values for Ctrl/Alt - Left on Ubuntu and Pi OS
    asciiArt->changeColumns(-1);
    break;
  case 558 ... 561: // Ctrl/Alt - Right
    asciiArt->changeColumns(+1);
    break;
  case 564 ... 567: // Ctrl/Alt - Up
    asciiArt->changeRows(-1);
    break;
  case 523 ... 526: // Ctrl/Alt - Down
    asciiArt->changeRows(+1);
    break;
  default:
    if (inch < 0x80)
    {
      ch = inch;
      // Standard for Apple II is Upper case
      if (ch >= 'A' && ch <= 'Z')
      {
	ch += 'a' - 'A';
      }
      else if (ch >= 'a' && ch <= 'z')
      {
	ch -= 'a' - 'A';
      }
    }
  }

  if (ch != ERR)
  {
    addKeyToBuffer(ch);
    return ERR;
  }
  else
  {
    // pass it back
    return inch;
  }
}

void ProcessInput()
{
  paddle->poll();
}

// Mockingboard
void registerSoundBuffer(IDirectSoundBuffer * buffer)
{
}

void unregisterSoundBuffer(IDirectSoundBuffer * buffer)
{
}
