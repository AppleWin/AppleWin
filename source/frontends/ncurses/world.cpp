#include "StdAfx.h"

#include <ncurses.h>
#include <signal.h>
#include <iostream>

#include "Common.h"
#include "Disk.h"
#include "Video.h"
#include "DiskImageHelper.h"
#include "Memory.h"

#include "linux/interface.h"

#include "frontends/ncurses/colors.h"

namespace
{
  std::shared_ptr<WINDOW> frame;
  std::shared_ptr<WINDOW> buffer;
  std::shared_ptr<WINDOW> borders;

  std::shared_ptr<GRColors> grColors;


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

  void sig_handler(int signo)
  {
    endwin();
    exit(1);
  }

  chtype mapCharacter(BYTE ch)
  {
    const bool normal = ch & 0x80;
    char low = ch & 0x7f;

    // just in case the following logic misses something
    chtype result = ACS_DIAMOND;

    if (low == 0x7f)
    {
      result = ACS_CKBOARD;
    }
    else
    {
      result = low;
      if (low >= 0x00 && low < 0x20)
      {
	result += 0x40;
      }
    }

    if (!normal)
    {
      result |= A_REVERSE;
    }

    return result;
  }

  void output(const char *fmt, ...)
  {
    va_list args;
    va_start(args, fmt);

    vwprintw(buffer.get(), fmt, args);
    wrefresh(buffer.get());
  }

}

bool Update40ColCell (int x, int y, int xpixel, int ypixel, int offset)
{
  BYTE ch = *(g_pTextBank0+offset);

  if (ch)
  {
    const chtype ch2 = mapCharacter(ch);
    mvwaddch(frame.get(), 1 + y, 1 + x, ch2);
  }

  return true;
}

bool Update80ColCell (int x, int y, int xpixel, int ypixel, int offset)
{
  return true;
}

bool UpdateLoResCell (int x, int y, int xpixel, int ypixel, int offset)
{
  BYTE val = *(g_pTextBank0+offset);

  const int pair = grColors->getPair(val);

  wattron(frame.get(), COLOR_PAIR(pair));
  mvwaddstr(frame.get(), 1 + y, 1 + x, "\u2580");
  wattroff(frame.get(), COLOR_PAIR(pair));

  return true;
}

bool UpdateDLoResCell (int x, int y, int xpixel, int ypixel, int offset)
{
  return true;
}

bool UpdateHiResCell (int x, int y, int xpixel, int ypixel, int offset)
{
  return true;
}

bool UpdateDHiResCell (int x, int y, int xpixel, int ypixel, int offset)
{
  return true;
}


void FrameRefresh()
{
  // std::cout << "Status Drive 1: " << g_eStatusDrive1 << ", Track: " << g_sTrackDrive1 << ", Sector: " << g_sSectorDrive1 << " ";
  // std::cout << "Status Drive 2: " << g_eStatusDrive2 << ", Track: " << g_sTrackDrive2 << ", Sector: " << g_sSectorDrive2 << " ";
  // std::cout << "\r" << std::flush;
}

void FrameDrawDiskLEDS(HDC x)
{
  DiskGetLightStatus(&g_eStatusDrive1, &g_eStatusDrive1);
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

  grColors.reset(new GRColors(20, 20));

  curs_set(0);
  signal(SIGINT, sig_handler);

  noecho();
  cbreak();
  set_escdelay(0);

  frame.reset(newwin(1 + 24 + 1, 1 + 40 + 1, 0, 0), delwin);
  box(frame.get(), 0 , 0);
  wtimeout(frame.get(), 0);
  keypad(frame.get(), true);
  wrefresh(frame.get());

  borders.reset(newwin(1 + 24 + 1, 1 + 40 + 1, 0, 43), delwin);
  box(borders.get(), 0 , 0);
  wrefresh(borders.get());

  buffer.reset(newwin(24, 40, 1, 44), delwin);
  scrollok(buffer.get(), true);
  wrefresh(buffer.get());
}

void VideoRedrawScreen()
{
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

  wrefresh(frame.get());
}

void ProcessKeyboard()
{
  int ch = wgetch(frame.get());
  if (ch != ERR)
  {
    switch (ch)
    {
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
    default:
      if (ch >= 0x80)
      {
	ch = ERR;
      }
      else
      {
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
    }
  }
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
