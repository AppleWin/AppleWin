#include "StdAfx.h"

#include <ncurses.h>
#include <signal.h>
#include <iostream>

#include "Common.h"
#include "Disk.h"
#include "Video.h"
#include "CPU.h"
#include "Log.h"
#include "DiskImageHelper.h"
#include "Memory.h"

#include "linux/interface.h"

#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/colors.h"
#include "frontends/ncurses/asciiart.h"
#include "frontends/ncurses/input.h"

namespace
{

  std::shared_ptr<Frame> frame;
  std::shared_ptr<GraphicsColors> colors;
  std::shared_ptr<ASCIIArt> asciiArt;

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

#define  SW_80COL         (g_uVideoMode & VF_80COL)
#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

  BYTE nextKey = 0;
  bool keyReady = false;

  bool g_bTextFlashState = false;

  double alpha = 10.0;
  double F = 0;

  void sig_handler(int signo)
  {
    // Ctrl-C
    // is there a race condition here?
    // is it a problem?
    nextKey = 0x83;
    keyReady = true;
  }

  chtype mapCharacter(BYTE ch)
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
      if (high == 0 && g_nAltCharSetOffset == 0)
      {
	result -= 0x40;
      }
      break;
    }

    if (result == 0x7f)
    {
      result = ACS_CKBOARD;
    }

    result != A_BLINK;

    if (!high)
    {
      if ((g_nAltCharSetOffset == 0) && (low >= 0x40))
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

  ULONG lastUpdate = 0;

  void updateSpeaker()
  {
    const ULONG dCycles = g_nCumulativeCycles - lastUpdate;
    const double dt = dCycles / CLK_6502;
    const double coeff = exp(- alpha * dt);
    F = F * coeff;
    lastUpdate = g_nCumulativeCycles;
  }

  typedef bool (*VideoUpdateFuncPtr_t)(int, int, int, int, int);

  bool Update40ColCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    frame->init(24, 40);
    asciiArt->init(1, 1);

    BYTE ch = *(g_pTextBank0+offset);

    WINDOW * win = frame->getWindow();

    const chtype ch2 = mapCharacter(ch);
    mvwaddch(win, 1 + y, 1 + x, ch2);

    return true;
  }

  bool Update80ColCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    frame->init(24, 80);
    asciiArt->init(1, 2);

    BYTE ch1 = *(g_pTextBank1+offset);
    BYTE ch2 = *(g_pTextBank0+offset);

    WINDOW * win = frame->getWindow();

    const chtype ch12 = mapCharacter(ch1);
    mvwaddch(win, 1 + y, 1 + 2 * x, ch12);

    const chtype ch22 = mapCharacter(ch2);
    mvwaddch(win, 1 + y, 1 + 2 * x + 1, ch22);

    return true;
  }

  bool UpdateLoResCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    BYTE val = *(g_pTextBank0+offset);

    const int pair = colors->getPair(val);

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

  bool UpdateDLoResCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

  bool UpdateHiResCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    const BYTE * base = g_pHiresBank0 + offset;

    const ASCIIArt::array_char_t & chs = asciiArt->getCharacters(base);

    const auto shape = chs.shape();
    const size_t rows = shape[0];
    const size_t cols = shape[1];

    frame->init(24 * rows, 40 * cols);
    WINDOW * win = frame->getWindow();

    for (size_t i = 0; i < rows; ++i)
    {
      for (size_t j = 0; j < cols; ++j)
      {
	const int pair = colors->getGrey(chs[i][j].foreground, chs[i][j].background);

	wcolor_set(win, pair, NULL);
	mvwaddstr(win, 1 + rows * y + i, 1 + cols * x + j, chs[i][j].c);
      }
    }
    wcolor_set(win, 0, NULL);

    return true;
  }

  bool UpdateDHiResCell (int x, int y, int xpixel, int ypixel, int offset)
  {
    return true;
  }

}

double g_relativeSpeed = 1.0;

void output(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  WINDOW * win = frame->getBuffer();

  vwprintw(win, fmt, args);
  wrefresh(win);
}

void FrameRefresh()
{
  WINDOW * status = frame->getStatus();

  mvwprintw(status, 1, 2, "D1: %d, %s, %s", g_eStatusDrive1, g_sTrackDrive1, g_sSectorDrive1);
  mvwprintw(status, 2, 2, "D2: %d, %s, %s", g_eStatusDrive2, g_sTrackDrive2, g_sSectorDrive2);

  // approximate
  const double frequency = 0.5 * alpha * F;
  mvwprintw(status, 1, 20, "%5.fHz", frequency);
  mvwprintw(status, 2, 20, "%5.1f%%", 100 * g_relativeSpeed);

  wrefresh(status);
}

void FrameDrawDiskLEDS(HDC x)
{
  DiskGetLightStatus(&g_eStatusDrive1, &g_eStatusDrive2);
  FrameRefresh();
}

void FrameDrawDiskStatus(HDC x)
{
  if (mem == NULL)
    return;

  // We use the actual drive since probing from memory doesn't tell us anything we don't already know.
  //        DOS3.3   ProDOS
  // Drive  $B7EA    $BE3D
  // Track  $B7EC    LC1 $D356
  // Sector $B7ED    LC1 $D357
  // RWTS            LC1 $D300
  int nActiveFloppy = DiskGetCurrentDrive();

  int nDisk1Track  = DiskGetTrack(0);
  int nDisk2Track  = DiskGetTrack(1);

  // Probe known OS's for Track/Sector
  int  isProDOS = mem[ 0xBF00 ] == 0x4C;
  bool isValid  = true;

  // Try DOS3.3 Sector
  if ( !isProDOS )
  {
    int nDOS33track  = mem[ 0xB7EC ];
    int nDOS33sector = mem[ 0xB7ED ];

    if ((nDOS33track  >= 0 && nDOS33track  < 40)
	&&  (nDOS33sector >= 0 && nDOS33sector < 16))
    {

      /**/ if (nActiveFloppy == 0) g_nSectorDrive1 = nDOS33sector;
      else if (nActiveFloppy == 1) g_nSectorDrive2 = nDOS33sector;
    }
    else
      isValid = false;
  }
  else // isProDOS
  {
    // we can't just read from mem[ 0xD357 ] since it might be bank-switched from ROM
    // and we need the Language Card RAM
    // memrom[ 0xD350 ] = " ERROR\x07\x00"  Applesoft error message
    //                             T   S
    int nProDOStrack  = *MemGetMainPtr( 0xC356 ); // LC1 $D356
    int nProDOSsector = *MemGetMainPtr( 0xC357 ); // LC1 $D357

    if ((nProDOStrack  >= 0 && nProDOStrack  < 40)
	&&  (nProDOSsector >= 0 && nProDOSsector < 16))
    {
      /**/ if (nActiveFloppy == 0) g_nSectorDrive1 = nProDOSsector;
      else if (nActiveFloppy == 1) g_nSectorDrive2 = nProDOSsector;
    }
    else
      isValid = false;
  }

  g_nTrackDrive1  = nDisk1Track;
  g_nTrackDrive2  = nDisk2Track;

  if( !isValid )
  {
    if (nActiveFloppy == 0) g_nSectorDrive1 = -1;
    else                    g_nSectorDrive2 = -1;
  }

  snprintf( g_sTrackDrive1 , sizeof(g_sTrackDrive1 ), "%2d", g_nTrackDrive1 );
  if (g_nSectorDrive1 < 0) snprintf( g_sSectorDrive1, sizeof(g_sSectorDrive1), "??" );
  else                     snprintf( g_sSectorDrive1, sizeof(g_sSectorDrive1), "%2d", g_nSectorDrive1 );

  snprintf( g_sTrackDrive2 , sizeof(g_sTrackDrive2),  "%2d", g_nTrackDrive2 );
  if (g_nSectorDrive2 < 0) snprintf( g_sSectorDrive2, sizeof(g_sSectorDrive2), "??" );
  else                     snprintf( g_sSectorDrive2, sizeof(g_sSectorDrive2), "%2d", g_nSectorDrive2 );

  FrameRefresh();
}

void FrameRefreshStatus(int x, bool)
{
  // std::cerr << "Status: " << x << std::endl;
}

void VideoInitialize()
{
  setlocale(LC_ALL, "");
  initscr();

  colors.reset(new GraphicsColors(20, 20, 32));

  curs_set(0);

  noecho();
  cbreak();
  set_escdelay(0);

  frame.reset(new Frame());
  asciiArt.reset(new ASCIIArt());
  Input::initialise("/dev/input/by-id/usb-©Microsoft_Corporation_Controller_1BBE3DB-event-joystick");

  signal(SIGINT, sig_handler);
}

void VideoUninitialize()
{
  endwin();
}


void VideoRedrawScreen()
{
  VideoUpdateFlash();
  updateSpeaker();
  FrameRefresh();

  const int displaypage2 = (SW_PAGE2) == 0 ? 0 : 1;

  g_pHiresBank1 = MemGetAuxPtr (0x2000 << displaypage2);
  g_pHiresBank0 = MemGetMainPtr(0x2000 << displaypage2);
  g_pTextBank1  = MemGetAuxPtr (0x400  << displaypage2);
  g_pTextBank0  = MemGetMainPtr(0x400  << displaypage2);

  VideoUpdateFuncPtr_t update = SW_TEXT
    ? SW_80COL
    ? Update80ColCell
    : Update40ColCell
    : SW_HIRES
    ? (SW_DHIRES && SW_80COL)
    ? UpdateDHiResCell
    : UpdateHiResCell
    : (SW_DHIRES && SW_80COL)
    ? UpdateDLoResCell
    : UpdateLoResCell;

  int  y        = 0;
  int  ypixel   = 0;
  while (y < 20) {
    int offset = ((y & 7) << 7) + ((y >> 3) * 40);
    int x      = 0;
    int xpixel = 0;
    while (x < 40) {
      update(x, y, xpixel, ypixel, offset + x);
      ++x;
      xpixel += 14;
    }
    ++y;
    ypixel += 16;
  }

  if (SW_MIXED)
    update = SW_80COL ? Update80ColCell
      : Update40ColCell;

  while (y < 24) {
    int offset = ((y & 7) << 7) + ((y >> 3) * 40);
    int x      = 0;
    int xpixel = 0;
    while (x < 40) {
      update(x, y, xpixel, ypixel, offset + x);
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
  const int inch = wgetch(frame->getWindow());

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
  case 0x221: // ALT - LEFT
    asciiArt->changeColumns(-1);
    break;
  case 0x230: // ALT - RIGHT
    asciiArt->changeColumns(+1);
    break;
  case 0x236:
    asciiArt->changeRows(-1);
    break;
  case 0x20D:
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
    nextKey = ch | 0x80;
    keyReady = true;
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
  Input::instance().poll();
}

BYTE    KeybGetKeycode ()
{
  return 0;
}

BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  return nextKey;
}

BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  BYTE result = keyReady ? nextKey : 0;
  nextKey = 0;
  return result;
}

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  CpuCalcCycles(nCyclesLeft);

  updateSpeaker();
  F += 1;

  return MemReadFloatingBus(nCyclesLeft);
}
