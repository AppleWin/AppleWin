/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Frame
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "MouseInterface.h"
#include "..\resource\resource.h"

#define ENABLE_MENU 0

// Magic numbers (used by FrameCreateWindow to calc width/height):
#define MAGICX 5	// 3D border between Apple window & Emulator's RHS buttons
#define MAGICY 5	// 3D border between Apple window & Title bar

#define  VIEWPORTCX  560
#if ENABLE_MENU
#define  VIEWPORTCY  400
#else
#define  VIEWPORTCY  384
#endif
#define  BUTTONX     (VIEWPORTCX + VIEWPORTX*2)
#define  BUTTONY     0
#define  BUTTONCX    45
#define  BUTTONCY    45
// NB. FSxxx = FullScreen xxx
#define  FSVIEWPORTX (640-BUTTONCX-MAGICX-VIEWPORTCX)
#define  FSVIEWPORTY ((480-VIEWPORTCY)/2)
#define  FSBUTTONX   (640-BUTTONCX)
#define  FSBUTTONY   (((480-VIEWPORTCY)/2)-1)
#define  BUTTONS     8

static HBITMAP capsbitmap[2];
static HBITMAP diskbitmap[ NUM_DISK_STATUS ];

static HBITMAP buttonbitmap[BUTTONS];

static bool    g_bAppActive = false;
static HBRUSH  btnfacebrush    = (HBRUSH)0;
static HPEN    btnfacepen      = (HPEN)0;
static HPEN    btnhighlightpen = (HPEN)0;
static HPEN    btnshadowpen    = (HPEN)0;
static int     buttonactive    = -1;
static int     buttondown      = -1;
static int     buttonover      = -1;
static int     buttonx         = BUTTONX;
static int     buttony         = BUTTONY;
static HRGN    clipregion      = (HRGN)0;
       HDC     g_hFrameDC         = (HDC)0;
static RECT    framerect       = {0,0,0,0};
HWND    g_hFrameWindow     = (HWND)0;
BOOL    fullscreen      = 0;
static BOOL    helpquit        = 0;
static BOOL    painting        = 0;
static HFONT   smallfont       = (HFONT)0;
static HWND    tooltipwindow   = (HWND)0;
static BOOL    usingcursor     = 0;
static int     viewportx       = VIEWPORTX;	// Default to Normal (non-FullScreen) mode
static int     viewporty       = VIEWPORTY;	// Default to Normal (non-FullScreen) mode

static LPDIRECTDRAW        directdraw = (LPDIRECTDRAW)0;
static LPDIRECTDRAWSURFACE surface    = (LPDIRECTDRAWSURFACE)0;

static bool g_bShowingCursor = true;
static bool g_bOldShowingCursor = true;	// Used during MODE_PAUSE
static bool g_bLastCursorInAppleViewport = false;

void    DrawStatusArea (HDC passdc, BOOL drawflags);
void    ProcessButtonClick (int button);
void	ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive);
void    RelayEvent (UINT message, WPARAM wparam, LPARAM lparam);
void    ResetMachineState ();
void    SetFullScreenMode ();
void    SetNormalMode ();
void    SetUsingCursor (BOOL);

bool	g_bScrollLock_FullSpeed = false;

// Prototypes:
static void FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen);
static void DrawCrosshairsMouse();
static void UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x=0, int y=0);

//===========================================================================
void CreateGdiObjects () {
  ZeroMemory(buttonbitmap,BUTTONS*sizeof(HBITMAP));
#define LOADBUTTONBITMAP(bitmapname)  LoadImage(g_hInstance,bitmapname,   \
                                                IMAGE_BITMAP,0,0,      \
                                                LR_CREATEDIBSECTION |  \
                                                LR_LOADMAP3DCOLORS |   \
                                                LR_LOADTRANSPARENT);
  buttonbitmap[BTN_HELP   ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("HELP_BUTTON"));
  buttonbitmap[BTN_RUN    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON"));
  buttonbitmap[BTN_DRIVE1 ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE1_BUTTON"));
  buttonbitmap[BTN_DRIVE2 ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE2_BUTTON"));
  buttonbitmap[BTN_DRIVESWAP] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVESWAP_BUTTON"));
  buttonbitmap[BTN_FULLSCR] = (HBITMAP)LOADBUTTONBITMAP(TEXT("FULLSCR_BUTTON"));
  buttonbitmap[BTN_DEBUG  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DEBUG_BUTTON"));
  buttonbitmap[BTN_SETUP  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("SETUP_BUTTON"));
  capsbitmap[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSOFF_BITMAP"));
  capsbitmap[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSON_BITMAP"));

  diskbitmap[ DISK_STATUS_OFF  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKOFF_BITMAP"));
  diskbitmap[ DISK_STATUS_READ ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKREAD_BITMAP"));
  diskbitmap[ DISK_STATUS_WRITE] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKWRITE_BITMAP"));
  diskbitmap[ DISK_STATUS_PROT ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKPROT_BITMAP"));
  
  btnfacebrush    = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
  btnfacepen      = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNFACE));
  btnhighlightpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));
  btnshadowpen    = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
  smallfont = CreateFont(11,6,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
                         OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
                         DEFAULT_QUALITY,VARIABLE_PITCH | FF_SWISS,
                         TEXT("Small Fonts"));
}

//===========================================================================
void DeleteGdiObjects () {
  int loop;
  for (loop = 0; loop < BUTTONS; loop++)
    DeleteObject(buttonbitmap[loop]);
  for (loop = 0; loop < 2; loop++)
    DeleteObject(capsbitmap[loop]);
  for (loop = 0; loop < NUM_DISK_STATUS; loop++)
    DeleteObject(diskbitmap[loop]);
  DeleteObject(btnfacebrush);
  DeleteObject(btnfacepen);
  DeleteObject(btnhighlightpen);
  DeleteObject(btnshadowpen);
  DeleteObject(smallfont);
}

// Draws an 3D box around the main apple screen
//===========================================================================
void Draw3dRect (HDC dc, int x1, int y1, int x2, int y2, BOOL out)
{	
	SelectObject(dc,GetStockObject(NULL_BRUSH));
	SelectObject(dc,out ? btnshadowpen : btnhighlightpen);
	POINT pt[3];
	pt[0].x = x1;    pt[0].y = y2-1;
	pt[1].x = x2-1;  pt[1].y = y2-1;
	pt[2].x = x2-1;  pt[2].y = y1; 
	Polyline(dc,(LPPOINT)&pt,3);
	SelectObject(dc,(out == 1) ? btnhighlightpen : btnshadowpen);
	pt[1].x = x1;    pt[1].y = y1;
	pt[2].x = x2;    pt[2].y = y1;
	Polyline(dc,(LPPOINT)&pt,3);
}

//===========================================================================
void DrawBitmapRect (HDC dc, int x, int y, LPRECT rect, HBITMAP bitmap) {
  HDC memdc = CreateCompatibleDC(dc);
  SelectObject(memdc,bitmap);
  BitBlt(dc,x,y,
         rect->right  + 1 - rect->left,
         rect->bottom + 1 - rect->top,
         memdc,
         rect->left,
         rect->top,
         SRCCOPY);
  DeleteDC(memdc);
}

//===========================================================================
void DrawButton (HDC passdc, int number) {
  FrameReleaseDC();
  HDC dc = (passdc ? passdc : GetDC(g_hFrameWindow));
  int x  = buttonx;
  int y  = buttony+number*BUTTONCY;
  if (number == buttondown) {
    int loop = 0;
    while (loop++ < 3)
      Draw3dRect(dc,x+loop,y+loop,x+BUTTONCX,y+BUTTONCY,0);
    RECT rect = {0,0,39,39};
    DrawBitmapRect(dc,x+4,y+4,&rect,buttonbitmap[number]);
  }
  else {
    Draw3dRect(dc,x+1,y+1,x+BUTTONCX,y+BUTTONCY,1);
    Draw3dRect(dc,x+2,y+2,x+BUTTONCX-1,y+BUTTONCY-1,1);
    RECT rect = {1,1,40,40};
    DrawBitmapRect(dc,x+3,y+3,&rect,buttonbitmap[number]);
  }
  if ((number == BTN_DRIVE1) || (number == BTN_DRIVE2)) {
    int  offset = (number == buttondown) << 1;
    RECT rect = {x+offset+3,
                 y+offset+31,
                 x+offset+42,
                 y+offset+42};
    SelectObject(dc,smallfont);
    SetTextColor(dc,RGB(0,0,0));
    SetTextAlign(dc,TA_CENTER | TA_TOP);
    SetBkMode(dc,TRANSPARENT);
    ExtTextOut(dc,x+offset+22,rect.top,ETO_CLIPPED,&rect,
               DiskGetName(number-BTN_DRIVE1),
               MIN(8,_tcslen(DiskGetName(number-BTN_DRIVE1))),
               NULL);
  }
  if (!passdc)
    ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================
void DrawCrosshairs (int x, int y) {
  static int lastx = 0;
  static int lasty = 0;
  FrameReleaseDC();
  HDC dc = GetDC(g_hFrameWindow);
#define LINE(x1,y1,x2,y2) MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);

  // ERASE THE OLD CROSSHAIRS
  if (lastx && lasty)
    if (fullscreen) {
      int loop = 4;
      while (loop--) {
        RECT rect = {0,0,5,5};
        switch (loop) {
          case 0: OffsetRect(&rect,lastx-2,FSVIEWPORTY-5);           break;
          case 1: OffsetRect(&rect,lastx-2,FSVIEWPORTY+VIEWPORTCY);  break;
          case 2: OffsetRect(&rect,FSVIEWPORTX-5,         lasty-2);  break;
          case 3: OffsetRect(&rect,FSVIEWPORTX+VIEWPORTCX,lasty-2);  break;
        }
        FillRect(dc,&rect,(HBRUSH)GetStockObject(BLACK_BRUSH));
      }
    }
    else {
      int loop = 5;
      while (loop--) {
        switch (loop) {
          case 0: SelectObject(dc,GetStockObject(BLACK_PEN));  break;
          case 1: // fall through
          case 2: SelectObject(dc,btnshadowpen);               break;
          case 3: // fall through
          case 4: SelectObject(dc,btnfacepen);                 break;
        }
        LINE(lastx-2,VIEWPORTY-loop-1,
             lastx+3,VIEWPORTY-loop-1);
        LINE(VIEWPORTX-loop-1,lasty-2,
             VIEWPORTX-loop-1,lasty+3);
        if ((loop == 1) || (loop == 2))
          SelectObject(dc,btnhighlightpen);
        LINE(lastx-2,VIEWPORTY+VIEWPORTCY+loop,
             lastx+3,VIEWPORTY+VIEWPORTCY+loop);
        LINE(VIEWPORTX+VIEWPORTCX+loop,lasty-2,
             VIEWPORTX+VIEWPORTCX+loop,lasty+3);
      }
    }

  // DRAW THE NEW CROSSHAIRS
  if (x && y) {
    int loop = 4;
    while (loop--) {
      if ((loop == 1) || (loop == 2))
        SelectObject(dc,GetStockObject(WHITE_PEN));
      else
        SelectObject(dc,GetStockObject(BLACK_PEN));
      LINE(x+loop-2,viewporty-5,
           x+loop-2,viewporty);
      LINE(x+loop-2,viewporty+VIEWPORTCY+4,
           x+loop-2,viewporty+VIEWPORTCY-1);
      LINE(viewportx-5,           y+loop-2,
           viewportx,             y+loop-2);
      LINE(viewportx+VIEWPORTCX+4,y+loop-2,
           viewportx+VIEWPORTCX-1,y+loop-2);
    }
  }
#undef LINE
  lastx = x;
  lasty = y;
  ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================
void DrawFrameWindow () {
  FrameReleaseDC();
  PAINTSTRUCT ps;
  HDC         dc = (painting ? BeginPaint(g_hFrameWindow,&ps)
                             : GetDC(g_hFrameWindow));
  VideoRealizePalette(dc);

  if (!fullscreen) {

    // DRAW THE 3D BORDER AROUND THE EMULATED SCREEN
    Draw3dRect(dc,
               VIEWPORTX-2,VIEWPORTY-2,
               VIEWPORTX+VIEWPORTCX+2,VIEWPORTY+VIEWPORTCY+2,
               0);
    Draw3dRect(dc,
               VIEWPORTX-3,VIEWPORTY-3,
               VIEWPORTX+VIEWPORTCX+3,VIEWPORTY+VIEWPORTCY+3,
               0);
    SelectObject(dc,btnfacepen);
    Rectangle(dc,
              VIEWPORTX-4,VIEWPORTY-4,
              VIEWPORTX+VIEWPORTCX+4,VIEWPORTY+VIEWPORTCY+4);
    Rectangle(dc,
              VIEWPORTX-5,VIEWPORTY-5,
              VIEWPORTX+VIEWPORTCX+5,VIEWPORTY+VIEWPORTCY+5);

    // DRAW THE TOOLBAR BUTTONS
    int loop = BUTTONS;
    while (loop--)
      DrawButton(dc,loop);
  }

  // DRAW THE STATUS AREA
  DrawStatusArea(dc,DRAW_BACKGROUND | DRAW_LEDS);
  if (painting)
    EndPaint(g_hFrameWindow,&ps);
  else
    ReleaseDC(g_hFrameWindow,dc);

  // DRAW THE CONTENTS OF THE EMULATED SCREEN
  if (g_nAppMode == MODE_LOGO)
    VideoDisplayLogo();
  else if (g_nAppMode == MODE_DEBUG)
    DebugDisplay(1);
  else
    VideoRedrawScreen();
}

//===========================================================================
void DrawStatusArea (HDC passdc, int drawflags)
{
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(g_hFrameWindow));
	int  x      = buttonx;
	int  y      = buttony+BUTTONS*BUTTONCY+1;
	int  iDrive1Status = DISK_STATUS_OFF;
	int  iDrive2Status = DISK_STATUS_OFF;
	bool bCaps   = KeybGetCapsStatus();
	DiskGetLightStatus(&iDrive1Status,&iDrive2Status);

	if (fullscreen)
	{
		SelectObject(dc,smallfont);
		SetBkMode(dc,OPAQUE);
		SetBkColor(dc,RGB(0,0,0));
		SetTextAlign(dc,TA_LEFT | TA_TOP);
		SetTextColor(dc,RGB((iDrive1Status==2 ? 255 : 0),(iDrive1Status==1 ? 255 : 0),0));
		TextOut(dc,x+ 3,y+2,TEXT("1"),1);
		SetTextColor(dc,RGB((iDrive2Status==2 ? 255 : 0),(iDrive2Status==1 ? 255 : 0),0));
		TextOut(dc,x+13,y+2,TEXT("2"),1);
		if (!IS_APPLE2)
		{
			SetTextAlign(dc,TA_RIGHT | TA_TOP);
			SetTextColor(dc,(bCaps
				? RGB(128,128,128)
				: RGB(  0,  0,  0) ));
			TextOut(dc,x+BUTTONCX,y+2,TEXT("Caps"),4);
		}
		SetTextAlign(dc,TA_CENTER | TA_TOP);
		SetTextColor(dc,(g_nAppMode == MODE_PAUSED || g_nAppMode == MODE_STEPPING
			? RGB(255,255,255)
			: RGB(  0,  0,  0)));
		TextOut(dc,x+BUTTONCX/2,y+13,(g_nAppMode == MODE_PAUSED
			? TITLE_PAUSED
			: TITLE_STEPPING) ,8);
	}
	else
	{
		if (drawflags & DRAW_BACKGROUND)
		{
			SelectObject(dc,GetStockObject(NULL_PEN));
			SelectObject(dc,btnfacebrush);
			Rectangle(dc,x,y,x+BUTTONCX+2,y+35);
			Draw3dRect(dc,x+1,y+3,x+BUTTONCX,y+31,0);
			SelectObject(dc,smallfont);
			SetTextAlign(dc,TA_CENTER | TA_TOP);
			SetTextColor(dc,RGB(0,0,0));
			SetBkMode(dc,TRANSPARENT);
			TextOut(dc,x+ 7,y+7,TEXT("1"),1);
			TextOut(dc,x+25,y+7,TEXT("2"),1);
		}
		if (drawflags & DRAW_LEDS)
		{
			RECT rect = {0,0,8,8};
			DrawBitmapRect(dc,x+12,y+8,&rect,diskbitmap[iDrive1Status]);
			DrawBitmapRect(dc,x+30,y+8,&rect,diskbitmap[iDrive2Status]);

			if (!IS_APPLE2)
			{
				RECT rect = {0,0,30,8};
				DrawBitmapRect(dc,x+7,y+19,&rect,capsbitmap[bCaps != 0]);
			}
		}

		if (drawflags & DRAW_TITLE)
		{
			TCHAR title[80];
			switch (g_Apple2Type)
			{
			case A2TYPE_APPLE2:			_tcscpy(title, TITLE_APPLE_2); break; 
			case A2TYPE_APPLE2PLUS:		_tcscpy(title, TITLE_APPLE_2_PLUS); break; 
			case A2TYPE_APPLE2E:		_tcscpy(title, TITLE_APPLE_2E); break; 
			case A2TYPE_APPLE2EEHANCED:	_tcscpy(title, TITLE_APPLE_2E_ENHANCED); break; 
			}

			if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
				_tcscat(title,TEXT(" (custom rom)"));
			else if (g_uTheFreezesF8Rom && IS_APPLE2)
				_tcscat(title,TEXT(" (The Freeze's non-autostart F8 rom)"));

			switch (g_nAppMode)
			{
				case MODE_PAUSED  : _tcscat(title,TEXT(" [")); _tcscat(title,TITLE_PAUSED  ); _tcscat(title,TEXT("]")); break;
				case MODE_STEPPING: _tcscat(title,TEXT(" [")); _tcscat(title,TITLE_STEPPING); _tcscat(title,TEXT("]")); break;
			}

			SendMessage(g_hFrameWindow,WM_SETTEXT,0,(LPARAM)title);
		}
		if (drawflags & DRAW_BUTTON_DRIVES)
		{
			DrawButton(dc, BTN_DRIVE1);
			DrawButton(dc, BTN_DRIVE2);
		}
	}

	if (!passdc)
		ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================
void EraseButton (int number) {
  RECT rect;
  rect.left   = buttonx;
  rect.right  = rect.left+BUTTONCX;
  rect.top    = buttony+number*BUTTONCY;
  rect.bottom = rect.top+BUTTONCY;
  InvalidateRect(g_hFrameWindow,&rect,1);
}

//===========================================================================
LRESULT CALLBACK FrameWndProc (
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam)
{
	switch (message)
	{
    case WM_ACTIVATE:
      JoyReset();
      SetUsingCursor(0);
      break;

    case WM_ACTIVATEAPP:
      g_bAppActive = (wparam ? true : false);
      break;

    case WM_CLOSE:
      if (fullscreen)
        SetNormalMode();
      if (!IsIconic(window))
        GetWindowRect(window,&framerect);
      RegSaveValue(TEXT("Preferences"),TEXT("Window X-Position"),1,framerect.left);
      RegSaveValue(TEXT("Preferences"),TEXT("Window Y-Position"),1,framerect.top);
      FrameReleaseDC();
      SetUsingCursor(0);
      if (helpquit) {
        helpquit = 0;
        HtmlHelp(NULL,NULL,HH_CLOSE_ALL,0);
      }
      break;

		case WM_CHAR:
			if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_LOGO) ||
				((g_nAppMode == MODE_STEPPING) && (wparam != TEXT('\x1B'))))
			{
				KeybQueueKeypress((int)wparam,ASCII);
			}
			else
			if ((g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING))
			{
				DebuggerInputConsoleChar((TCHAR)wparam);
			}
			break;

    case WM_CREATE:
      g_hFrameWindow = window;
      CreateGdiObjects();
	  DSInit();
	  DIMouse::DirectInputInit(window);
      MB_Initialize();
      SpkrInitialize();
      DragAcceptFiles(window,1);
      break;

    case WM_DDE_INITIATE: {
      ATOM application = GlobalAddAtom(TEXT("applewin"));
      ATOM topic       = GlobalAddAtom(TEXT("system"));
      if(LOWORD(lparam) == application && HIWORD(lparam) == topic)
        SendMessage((HWND)wparam,WM_DDE_ACK,(WPARAM)window,MAKELPARAM(application,topic));
      GlobalDeleteAtom(application);
      GlobalDeleteAtom(topic);
      break;
    }

    case WM_DDE_EXECUTE: {
      LPTSTR filename = (LPTSTR)GlobalLock((HGLOBAL)lparam);
      int error = DiskInsert(0,filename,0,0);
      if (!error) {
        if (!fullscreen)
          DrawButton((HDC)0,BTN_DRIVE1);
        SetForegroundWindow(window);
        ProcessButtonClick(BTN_RUN);
      }
      else
        DiskNotifyInvalidImage(filename,error);
      GlobalUnlock((HGLOBAL)lparam);
      break;
    }

    case WM_DESTROY:
      DragAcceptFiles(window,0);
	  Snapshot_Shutdown();
      DebugDestroy();
      if (!restart) {
        DiskDestroy();
        ImageDestroy();
        HD_Cleanup();
      }
      PrintDestroy();
      sg_SSC.CommDestroy();
      CpuDestroy();
      MemDestroy();
      SpkrDestroy();
      VideoDestroy();
      MB_Destroy();
      DeleteGdiObjects();
      DIMouse::DirectInputUninit(window);
      PostQuitMessage(0);	// Post WM_QUIT message to the thread's message queue
      break;

    case WM_DISPLAYCHANGE:
      VideoReinitialize();
      break;

    case WM_DROPFILES: {
      TCHAR filename[MAX_PATH];
      DragQueryFile((HDROP)wparam,0,filename,sizeof(filename));
      POINT point;
      DragQueryPoint((HDROP)wparam,&point);
      RECT rect;
      rect.left   = buttonx;
      rect.right  = rect.left+BUTTONCX+1;
      rect.top    = buttony+BTN_DRIVE2*BUTTONCY+1;
      rect.bottom = rect.top+BUTTONCY;
      int error = DiskInsert(PtInRect(&rect,point) ? 1 : 0,filename,0,0);
      if (!error) {
        if (!fullscreen)
          DrawButton((HDC)0,PtInRect(&rect,point) ? BTN_DRIVE2 : BTN_DRIVE1);
        rect.top = buttony+BTN_DRIVE1*BUTTONCY+1;
        if (!PtInRect(&rect,point)) {
          SetForegroundWindow(window);
          ProcessButtonClick(BTN_RUN);
        }
      }
      else
        DiskNotifyInvalidImage(filename,error);
      DragFinish((HDROP)wparam);
      break;
    }

	case WM_KEYDOWN:
		KeybUpdateCtrlShiftStatus();
		if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == -1))
		{
			SetUsingCursor(0);
			buttondown = wparam-VK_F1;
			if (fullscreen && (buttonover != -1)) {
				if (buttonover != buttondown)
				EraseButton(buttonover);
				buttonover = -1;
			}
			DrawButton((HDC)0,buttondown);
		}
		else if (wparam == VK_F9)
		{
			// Cycle through available video modes
			if (GetKeyState(VK_SHIFT) >= 0)	// Backwards
			{
				if (videotype == 0)
					videotype = VT_NUM_MODES;
				videotype--;
			}
			else							// Forwards
			{
				videotype++;
				if (videotype >= VT_NUM_MODES)
					videotype = 0;
			}

			VideoReinitialize();
			if ((g_nAppMode != MODE_LOGO) || ((g_nAppMode == MODE_DEBUG) && (g_bDebuggerViewingAppleOutput))) // +PATCH
			{
				VideoRedrawScreen();
				g_bDebuggerViewingAppleOutput = true;  // +PATCH
			}
			RegSaveValue(TEXT("Configuration"),TEXT("Video Emulation"),1,videotype);
		}
		else if ((wparam == VK_F11) && (GetKeyState(VK_CONTROL) >= 0))	// Save state (F11)
		{
			SoundCore_SetFade(FADE_OUT);
			if(PSP_SaveStateSelectImage(window, true))
			{
				Snapshot_SaveState();
			}
			SoundCore_SetFade(FADE_IN);
		}
		else if (wparam == VK_F12)										// Load state (F12 or Ctrl+F12)
		{
			SoundCore_SetFade(FADE_OUT);
			if(PSP_SaveStateSelectImage(window, false))
			{
				Snapshot_LoadState();
			}
			SoundCore_SetFade(FADE_IN);
		}
		else if (wparam == VK_CAPITAL)
		{
			KeybToggleCapsLock();
		}
		else if (wparam == VK_PAUSE)
		{
			SetUsingCursor(0);
			switch (g_nAppMode)
			{
				case MODE_RUNNING:
					g_nAppMode = MODE_PAUSED;
					SoundCore_SetFade(FADE_OUT);
					g_bOldShowingCursor = g_bShowingCursor;
					if (sg_Mouse.IsActiveAndEnabled() && !g_bShowingCursor)
					{
						int nCount = ShowCursor(1);
						_ASSERT(nCount >= 0);
						g_bShowingCursor = true;

						if (g_uMouseShowCrosshair)	// Erase crosshairs if they are being drawn
							DrawCrosshairs(0,0);
					}
					break;
				case MODE_PAUSED:
					g_nAppMode = MODE_RUNNING;
					SoundCore_SetFade(FADE_IN);
					if (sg_Mouse.IsActiveAndEnabled() && !g_bOldShowingCursor)
					{
						int nCount = ShowCursor(0);
						_ASSERT(nCount < 0);
						g_bShowingCursor = false;
					}
					break;
				case MODE_STEPPING:
					DebuggerInputConsoleChar( DEBUG_EXIT_KEY );
					break;
			}
			DrawStatusArea((HDC)0,DRAW_TITLE);
			if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
				VideoRedrawScreen();
			g_bResetTiming = true;
		}
		else if ((wparam == VK_SCROLL) && g_uScrollLockToggle)
		{
			g_bScrollLock_FullSpeed = !g_bScrollLock_FullSpeed;
		}
		else if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_LOGO) || (g_nAppMode == MODE_STEPPING))
		{
			// Note about Alt Gr (Right-Alt):
			// . WM_KEYDOWN[Left-Control], then:
			// . WM_KEYDOWN[Right-Alt]
			BOOL autorep  = ((lparam & 0x40000000) != 0);
			BOOL extended = ((lparam & 0x01000000) != 0);
			if ((!JoyProcessKey((int)wparam,extended,1,autorep)) && (g_nAppMode != MODE_LOGO))
				KeybQueueKeypress((int)wparam,NOT_ASCII);
		}
		else if (g_nAppMode == MODE_DEBUG)
			DebuggerProcessKey(wparam);


		if (wparam == VK_F10)
		{
			SetUsingCursor(0);
			return 0;
		}
		break;

    case WM_KEYUP:
		if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == (int)wparam-VK_F1))
		{
			buttondown = -1;
			if (fullscreen)
				EraseButton(wparam-VK_F1);
			else
				DrawButton((HDC)0,wparam-VK_F1);
			ProcessButtonClick(wparam-VK_F1);
		}
		else
		{
			JoyProcessKey((int)wparam,((lparam & 0x01000000) != 0),0,0);
		}
		break;

    case WM_LBUTTONDOWN:
      if (buttondown == -1)
	  {
        int x = LOWORD(lparam);
        int y = HIWORD(lparam);
        if ((x >= buttonx) &&
            (y >= buttony) &&
            (y <= buttony+BUTTONS*BUTTONCY))
		{
          buttonactive = buttondown = (y-buttony-1)/BUTTONCY;
          DrawButton((HDC)0,buttonactive);
          SetCapture(window);
        }
        else if (usingcursor)
		{
          if (wparam & (MK_CONTROL | MK_SHIFT))
		  {
            SetUsingCursor(0);
		  }
          else
		  {
	        JoySetButton(BUTTON0, BUTTON_DOWN);
		  }
		}
        else if ( ((x < buttonx) && JoyUsingMouse() && ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))) )
		{
          SetUsingCursor(1);
		}
		else if (sg_Mouse.IsActive())
		{
			if (wparam & (MK_CONTROL | MK_SHIFT))
			{
				sg_Mouse.SetEnabled(false);

				int nCount = ShowCursor(1);
				_ASSERT(nCount >= 0);
				g_bShowingCursor = true;

				if (g_uMouseShowCrosshair)	// Erase crosshairs if they are being drawn
					DrawCrosshairs(0,0);
			}
			else
			{
				if (!sg_Mouse.IsEnabled())
				{
					sg_Mouse.SetEnabled(true);

					int nCount = ShowCursor(0);
					_ASSERT(nCount < 0);
					g_bShowingCursor = false;
					// Don't call SetButton() when enabling
				}
				else
				{
					sg_Mouse.SetButton(BUTTON0, BUTTON_DOWN);
				}
			}
		}
		DebuggerMouseClick( x, y );
      }
      RelayEvent(WM_LBUTTONDOWN,wparam,lparam);
      break;

    case WM_LBUTTONUP:
      if (buttonactive != -1) {
        ReleaseCapture();
        if (buttondown == buttonactive) {
          buttondown = -1;
          if (fullscreen)
            EraseButton(buttonactive);
          else
            DrawButton((HDC)0,buttonactive);
          ProcessButtonClick(buttonactive);
        }
        buttonactive = -1;
      }
      else if (usingcursor)
	  {
	    JoySetButton(BUTTON0, BUTTON_UP);
	  }
	  else if (sg_Mouse.IsActive())
	  {
		sg_Mouse.SetButton(BUTTON0, BUTTON_UP);
	  }
      RelayEvent(WM_LBUTTONUP,wparam,lparam);
      break;

    case WM_MOUSEMOVE: {
      int x = LOWORD(lparam);
      int y = HIWORD(lparam);
      int newover = (((x >= buttonx) &&
                      (x <= buttonx+BUTTONCX) &&
                      (y >= buttony) &&
                      (y <= buttony+BUTTONS*BUTTONCY))
                     ? (y-buttony-1)/BUTTONCY : -1);
      if (buttonactive != -1) {
        int newdown = (newover == buttonactive) ? buttonactive : -1;
        if (newdown != buttondown) {
          buttondown = newdown;
          DrawButton((HDC)0,buttonactive);
        }
      }
      else if (fullscreen && (newover != buttonover) && (buttondown == -1)) {
        if (buttonover != -1)
          EraseButton(buttonover);
        buttonover = newover;
        if (buttonover != -1)
          DrawButton((HDC)0,buttonover);
      }
      else if (usingcursor)
	  {
        DrawCrosshairs(x,y);
	    JoySetPosition(x-viewportx-2, VIEWPORTCX-4, y-viewporty-2, VIEWPORTCY-4);
      }
	  else if (sg_Mouse.IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING))
	  {
			if (g_bLastCursorInAppleViewport)
				break;

			const int iAppleScreenMaxX = VIEWPORTCX-1;
			const int iAppleScreenMaxY = VIEWPORTCY-1;
			const int iBoundMinX = viewportx;
			const int iBoundMaxX = iAppleScreenMaxX;
			const int iBoundMinY = viewporty;
			const int iBoundMaxY = iAppleScreenMaxY;

			int iOutOfBoundsX=0, iOutOfBoundsY=0;
			if (x < iBoundMinX)	iOutOfBoundsX=-1;
			if (x > iBoundMaxX)	iOutOfBoundsX=1;
			if (y < iBoundMinY)	iOutOfBoundsY=-1;
			if (y > iBoundMaxY)	iOutOfBoundsY=1;

			UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY, x, y);
	  }

      RelayEvent(WM_MOUSEMOVE,wparam,lparam);
      break;
    }

	case WM_TIMER:
		if (wparam == IDEVENT_TIMER_MOUSE)
		{
			if (sg_Mouse.IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING))
			{
				if (g_bLastCursorInAppleViewport == false)
					break;

				int iOutOfBoundsX=0, iOutOfBoundsY=0;

				long dX,dY;
				if (DIMouse::ReadImmediateData(&dX, &dY) == S_OK)
					sg_Mouse.SetPositionRel(dX, dY, &iOutOfBoundsX, &iOutOfBoundsY);

				UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY);
			}
		}
		break;

// VSCROLL 
// SB_LINEUP // Line Scrolling
// SB_PAGEUP // Page Scrolling
	case WM_MOUSEWHEEL:
		if (g_nAppMode == MODE_DEBUG)
		{
			KeybUpdateCtrlShiftStatus();
			int zDelta = (short) HIWORD( wparam );
			if (zDelta > 0)
			{
				DebuggerProcessKey( VK_UP );
			}
			else
			{
				DebuggerProcessKey( VK_DOWN );
			}
		}
		break;

    case WM_NOTIFY:
      if(((LPNMTTDISPINFO)lparam)->hdr.hwndFrom == tooltipwindow &&
         ((LPNMTTDISPINFO)lparam)->hdr.code == TTN_GETDISPINFO)
        ((LPNMTTDISPINFO)lparam)->lpszText =
          (LPTSTR)DiskGetFullName(((LPNMTTDISPINFO)lparam)->hdr.idFrom);
      break;

    case WM_PAINT:
      if (GetUpdateRect(window,NULL,0)) {
        painting = 1;
        DrawFrameWindow();
        painting = 0;
      }
      break;

    case WM_PALETTECHANGED:
      if ((HWND)wparam == window)
        break;
      // fall through

    case WM_QUERYNEWPALETTE:
      DrawFrameWindow();
      break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
		// Right Click on Drive Icon -- eject Disk
		if ((buttonover == -1) && (message == WM_RBUTTONUP)) // HACK: BUTTON_NONE
		{
			int x = LOWORD(lparam);
			int y = HIWORD(lparam);

			if ((x >= buttonx) &&
				(y >= buttony) &&
				(y <= buttony+BUTTONS*BUTTONCY))
			{
				int iButton = (y-buttony-1)/BUTTONCY;
				int iDrive = iButton - BTN_DRIVE1;
				if ((iButton == BTN_DRIVE1) || (iButton == BTN_DRIVE2))
				{
/*
					if (KeybGetShiftStatus())
						DiskProtect( iDrive, true );
					else
					if (KeybGetCtrlStatus())
						DiskProtect( iDrive, false );
					else
*/
					{
						RECT  rect;    // client area             
						POINT pt;   // location of mouse click  

   						// Get the bounding rectangle of the client area. 
						GetClientRect(window, (LPRECT) &rect); 
			 
						// Get the client coordinates for the mouse click.  
						pt.x = GET_X_LPARAM(lparam); 
						pt.y = GET_Y_LPARAM(lparam); 
			 
						// If the mouse click took place inside the client 
						// area, execute the application-defined function 
						// that displays the shortcut menu. 
						if (PtInRect((LPRECT) &rect, pt)) 
							ProcessDiskPopupMenu( window, pt, iDrive ); 
                	}

					FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
					DrawButton((HDC)0,iButton);
				}			
			}
		}
		if (usingcursor)
		{
			if (sg_Mouse.IsActive())
				sg_Mouse.SetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);
			else
				JoySetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);
		}
		RelayEvent(message,wparam,lparam);
		break;

    case WM_SYSCOLORCHANGE:
      DeleteGdiObjects();
      CreateGdiObjects();
      break;

    case WM_SYSCOMMAND:
      switch (wparam & 0xFFF0) {
        case SC_KEYMENU:
          if (fullscreen && g_bAppActive)
            return 0;
          break;
        case SC_MINIMIZE:
          GetWindowRect(window,&framerect);
          break;
      }
      break;

    case WM_SYSKEYDOWN:
      PostMessage(window,WM_KEYDOWN,wparam,lparam);
      if ((wparam == VK_F10) || (wparam == VK_MENU))	// VK_MENU == ALT Key
        return 0;
      break;

    case WM_SYSKEYUP:
      PostMessage(window,WM_KEYUP,wparam,lparam);
      break;

    case WM_USER_BENCHMARK: {
      if (g_nAppMode != MODE_LOGO)
        if (MessageBox(g_hFrameWindow,
                       TEXT("Running the benchmarks will reset the state of ")
                       TEXT("the emulated machine, causing you to lose any ")
                       TEXT("unsaved work.\n\n")
                       TEXT("Are you sure you want to do this?"),
                       TEXT("Benchmarks"),
                       MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDNO)
          break;
      UpdateWindow(window);
      ResetMachineState();
      g_nAppMode = MODE_LOGO;
      DrawStatusArea((HDC)0,DRAW_TITLE);
      HCURSOR oldcursor = SetCursor(LoadCursor(0,IDC_WAIT));
      VideoBenchmark();
      ResetMachineState();
      SetCursor(oldcursor);
      break;
    }

    case WM_USER_RESTART:
	  // . Changed Apple computer type (][+ or //e)
	  // . Changed disk speed (normal or enhanced)
	  // . Changed Freeze F8 rom setting
      if (g_nAppMode != MODE_LOGO)
        if (MessageBox(g_hFrameWindow,
                       TEXT("Restarting the emulator will reset the state ")
                       TEXT("of the emulated machine, causing you to lose any ")
                       TEXT("unsaved work.\n\n")
                       TEXT("Are you sure you want to do this?"),
                       TEXT("Configuration"),
                       MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDNO)
          break;
      restart = 1;
      PostMessage(window,WM_CLOSE,0,0);
      break;

    case WM_USER_SAVESTATE:		// Save state
		Snapshot_SaveState();
		break;

    case WM_USER_LOADSTATE:		// Load state
		Snapshot_LoadState();
		break;
  }
  return DefWindowProc(window,message,wparam,lparam);
}



//===========================================================================
void ProcessButtonClick (int button) {

  SoundCore_SetFade(FADE_OUT);

  switch (button) {

    case BTN_HELP:
      {
        TCHAR filename[MAX_PATH];
        _tcscpy(filename,g_sProgramDir);
        _tcscat(filename,TEXT("APPLEWIN.CHM"));
        HtmlHelp(g_hFrameWindow,filename,HH_DISPLAY_TOC,0);
        helpquit = 1;
      }
      break;

    case BTN_RUN:
      if (g_nAppMode == MODE_LOGO)
        DiskBoot();
      else if (g_nAppMode == MODE_RUNNING)
        ResetMachineState();
      if ((g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING))
        DebugEnd();
      g_nAppMode = MODE_RUNNING;
      DrawStatusArea((HDC)0,DRAW_TITLE);
      VideoRedrawScreen();
      g_bResetTiming = true;
      break;

    case BTN_DRIVE1:
    case BTN_DRIVE2:
      DiskSelect(button-BTN_DRIVE1);
      if (!fullscreen)
        DrawButton((HDC)0,button);
      break;

    case BTN_DRIVESWAP:
      DiskDriveSwap();
      break;

    case BTN_FULLSCR:
      if (fullscreen)
        SetNormalMode();
      else
        SetFullScreenMode();
      break;

    case BTN_DEBUG:
		if (g_nAppMode == MODE_LOGO)
		{
			ResetMachineState();
		}

		if (g_nAppMode == MODE_STEPPING)
		{
			DebuggerInputConsoleChar( DEBUG_EXIT_KEY );
		}
		else
		if (g_nAppMode == MODE_DEBUG)
		{
			g_bDebugDelayBreakCheck = true;
			ProcessButtonClick(BTN_RUN);
		}
		else
		{
			DebugBegin();
		}
      break;

    case BTN_SETUP:
      {
		  PSP_Init();
      }
      break;

  }

  if((g_nAppMode != MODE_DEBUG) && (g_nAppMode != MODE_PAUSED))
  {
	  SoundCore_SetFade(FADE_IN);
  }
}


//===========================================================================
void ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive) 
{
	// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/winui/windowsuserinterface/resources/menus/usingmenus.asp
	// http://www.codeproject.com/menu/MenusForBeginners.asp?df=100&forumid=67645&exp=0&select=903061

	HMENU hmenu;            // menu template
	HMENU hmenuTrackPopup;  // shortcut menu

	//  Load the menu template containing the shortcut menu from the 
	//  application's resources. 
	hmenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(ID_MENU_DISK_POPUP)); 
	if (hmenu == NULL) 
		return; 

	// Get the first shortcut menu in the menu template. This is the 
	// menu that TrackPopupMenu displays. 
	hmenuTrackPopup = GetSubMenu(hmenu, 0); 

	// TrackPopup uses screen coordinates, so convert the 
	// coordinates of the mouse click to screen coordinates. 
	ClientToScreen(hwnd, (LPPOINT) &pt); 

	int iMenuItem = ID_DISKMENU_WRITEPROTECTION_OFF;
	if (DiskGetProtect( iDrive ))
		iMenuItem = ID_DISKMENU_WRITEPROTECTION_ON;

	CheckMenuItem( hmenu,
		iMenuItem,
		MF_CHECKED // MF_BYPOSITION     
	);

	// Draw and track the shortcut menu.  
	int iCommand = TrackPopupMenu(
		hmenuTrackPopup
		, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD
		, pt.x, pt.y
		, 0
		, hwnd, NULL ); 

	if (iCommand == ID_DISKMENU_EJECT)
		DiskEject( iDrive );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_ON)
		DiskSetProtect( iDrive, true );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_OFF)
		DiskSetProtect( iDrive, false );

	// Destroy the menu. 
	DestroyMenu(hmenu); 
} 


//===========================================================================
void RelayEvent (UINT message, WPARAM wparam, LPARAM lparam) {
  if (fullscreen)
    return;
  MSG msg;
  msg.hwnd    = g_hFrameWindow;
  msg.message = message;
  msg.wParam  = wparam;
  msg.lParam  = lparam;
  SendMessage(tooltipwindow,TTM_RELAYEVENT,0,(LPARAM)&msg);
}

//===========================================================================
void ResetMachineState () {
  DiskReset();		// Set floppymotoron=0
  g_bFullSpeed = 0;	// Might've hit reset in middle of InternalCpuExecute() - so beep may get (partially) muted

  MemReset();
  DiskBoot();
  VideoResetState();
  sg_SSC.CommReset();
  PrintReset();
  JoyReset();
  MB_Reset();
  SpkrReset();
  sg_Mouse.Reset();

  SoundCore_SetFade(FADE_NONE);
}

//===========================================================================
void SetFullScreenMode () {
  fullscreen = 1;
  buttonover = -1;
  buttonx    = FSBUTTONX;
  buttony    = FSBUTTONY;
  viewportx  = FSVIEWPORTX;
  viewporty  = FSVIEWPORTY;
  GetWindowRect(g_hFrameWindow,&framerect);
  SetWindowLong(g_hFrameWindow,GWL_STYLE,WS_POPUP | WS_SYSMENU | WS_VISIBLE);
  DDSURFACEDESC ddsd;
  ddsd.dwSize = sizeof(ddsd);
  ddsd.dwFlags = DDSD_CAPS;
  ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
  if (DirectDrawCreate(NULL,&directdraw,NULL) != DD_OK ||
      directdraw->SetCooperativeLevel(g_hFrameWindow,DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK ||
      directdraw->SetDisplayMode(640,480,8) != DD_OK ||
      directdraw->CreateSurface(&ddsd,&surface,NULL) != DD_OK) {
    SetNormalMode();
    return;
  }
  InvalidateRect(g_hFrameWindow,NULL,1);
}

//===========================================================================
void SetNormalMode () {
  fullscreen = 0;
  buttonover = -1;
  buttonx    = BUTTONX;
  buttony    = BUTTONY;
  viewportx  = VIEWPORTX;
  viewporty  = VIEWPORTY;
  directdraw->RestoreDisplayMode();
  directdraw->SetCooperativeLevel(NULL,DDSCL_NORMAL);
  SetWindowLong(g_hFrameWindow,GWL_STYLE,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                WS_VISIBLE);
  SetWindowPos(g_hFrameWindow,0,framerect.left,
                             framerect.top,
                             framerect.right - framerect.left,
                             framerect.bottom - framerect.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);
  if (surface) {
    surface->Release();
    surface = (LPDIRECTDRAWSURFACE)0;
  }
  directdraw->Release();
  directdraw = (LPDIRECTDRAW)0;
}

//===========================================================================
void SetUsingCursor (BOOL newvalue) {
  if (newvalue == usingcursor)
    return;
  usingcursor = newvalue;
  if (usingcursor) {
    SetCapture(g_hFrameWindow);
    RECT rect = {viewportx+2,				// left
                 viewporty+2,				// top
                 viewportx+VIEWPORTCX-1,	// right
                 viewporty+VIEWPORTCY-1};	// bottom
    ClientToScreen(g_hFrameWindow,(LPPOINT)&rect.left);
    ClientToScreen(g_hFrameWindow,(LPPOINT)&rect.right);
    ClipCursor(&rect);
    ShowCursor(0);
    POINT pt;
    GetCursorPos(&pt);
    ScreenToClient(g_hFrameWindow,&pt);
    DrawCrosshairs(pt.x,pt.y);
  }
  else {
    DrawCrosshairs(0,0);
    ShowCursor(1);
    ClipCursor(NULL);
    ReleaseCapture();
  }
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void FrameCreateWindow ()
{
	int width  = VIEWPORTCX + VIEWPORTX*2
							+ BUTTONCX
							+ GetSystemMetrics(SM_CXBORDER)*2
							+ MAGICX;
	int height = VIEWPORTCY + VIEWPORTY*2
							+ GetSystemMetrics(SM_CYBORDER)
							+ GetSystemMetrics(SM_CYCAPTION)
							+ MAGICY;
	int xpos;

	if (!RegLoadValue(TEXT("Preferences"),TEXT("Window X-Position"),1,(DWORD *)&xpos))
		xpos = (GetSystemMetrics(SM_CXSCREEN)-width) >> 1;

	int ypos;
	if (!RegLoadValue(TEXT("Preferences"),TEXT("Window Y-Position"),1,(DWORD *)&ypos))
		ypos = (GetSystemMetrics(SM_CYSCREEN)-height) >> 1;

	switch (g_Apple2Type)
	{
	case A2TYPE_APPLE2:			g_pAppTitle = TITLE_APPLE_2; break; 
	case A2TYPE_APPLE2PLUS:		g_pAppTitle = TITLE_APPLE_2_PLUS; break; 
	case A2TYPE_APPLE2E:		g_pAppTitle = TITLE_APPLE_2E; break; 
	case A2TYPE_APPLE2EEHANCED:	g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break; 
	}

	g_hFrameWindow = CreateWindow(
		TEXT("APPLE2FRAME"),
		g_pAppTitle,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_VISIBLE,
		xpos,ypos,width,height,
		HWND_DESKTOP,
		(HMENU)0,
		g_hInstance,NULL );


	InitCommonControls();
	tooltipwindow = CreateWindow(
		TOOLTIPS_CLASS,NULL,TTS_ALWAYSTIP, 
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, 
		g_hFrameWindow,
		(HMENU)0,
		g_hInstance,NULL ); 

	TOOLINFO toolinfo;
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.uFlags = TTF_CENTERTIP;
	toolinfo.hwnd = g_hFrameWindow;
	toolinfo.hinst = g_hInstance;
	toolinfo.lpszText = LPSTR_TEXTCALLBACK;
	toolinfo.rect.left  = BUTTONX;
	toolinfo.rect.right = toolinfo.rect.left+BUTTONCX+1;
	toolinfo.uId = 0;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE1*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow,TTM_ADDTOOL,0,(LPARAM)&toolinfo);
	toolinfo.uId = 1;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE2*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow,TTM_ADDTOOL,0,(LPARAM)&toolinfo);
}

//===========================================================================
HDC FrameGetDC () {
  if (!g_hFrameDC) {
    g_hFrameDC = GetDC(g_hFrameWindow);
    SetViewportOrgEx(g_hFrameDC,viewportx,viewporty,NULL);
  }
  return g_hFrameDC;
}

//===========================================================================
HDC FrameGetVideoDC (LPBYTE *addr, LONG *pitch)
{
	if (fullscreen && g_bAppActive && !painting)
	{
		RECT rect = {	FSVIEWPORTX,
						FSVIEWPORTY,
						FSVIEWPORTX+VIEWPORTCX,
						FSVIEWPORTY+VIEWPORTCY};
		DDSURFACEDESC surfacedesc;
		surfacedesc.dwSize = sizeof(surfacedesc);
		// TC: Use DDLOCK_WAIT - see Bug #13425
		if (surface->Lock(&rect,&surfacedesc,DDLOCK_WAIT,NULL) == DDERR_SURFACELOST)
		{
			surface->Restore();
			surface->Lock(&rect,&surfacedesc,DDLOCK_WAIT,NULL);
		}
		*addr  = (LPBYTE)surfacedesc.lpSurface+(VIEWPORTCY-1)*surfacedesc.lPitch;
		*pitch = -surfacedesc.lPitch;
		return (HDC)0;
	}
	else
	{
		return FrameGetDC();
	}
}

//===========================================================================
void FrameRefreshStatus (int drawflags) {
  DrawStatusArea((HDC)0,drawflags);
}

//===========================================================================
void FrameRegisterClass () {
  WNDCLASSEX wndclass;
  ZeroMemory(&wndclass,sizeof(WNDCLASSEX));
  wndclass.cbSize        = sizeof(WNDCLASSEX);
  wndclass.style         = CS_OWNDC | CS_BYTEALIGNCLIENT;
  wndclass.lpfnWndProc   = FrameWndProc;
  wndclass.hInstance     = g_hInstance;
  wndclass.hIcon         = LoadIcon(g_hInstance,TEXT("APPLEWIN_ICON"));
  wndclass.hCursor       = LoadCursor(0,IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
#if ENABLE_MENU
  wndclass.lpszMenuName	 = (LPCSTR)IDR_MENU1;
#endif
  wndclass.lpszClassName = TEXT("APPLE2FRAME");
  wndclass.hIconSm       = (HICON)LoadImage(g_hInstance,TEXT("APPLEWIN_ICON"),
                                            IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
  RegisterClassEx(&wndclass);
}

//===========================================================================
void FrameReleaseDC () {
  if (g_hFrameDC) {
    SetViewportOrgEx(g_hFrameDC,0,0,NULL);
    ReleaseDC(g_hFrameWindow,g_hFrameDC);
    g_hFrameDC = (HDC)0;
  }
}

//===========================================================================
void FrameReleaseVideoDC () {
  if (fullscreen && g_bAppActive && !painting) {

    // THIS IS CORRECT ACCORDING TO THE DIRECTDRAW DOCS
    RECT rect = {FSVIEWPORTX,
                 FSVIEWPORTY,
                 FSVIEWPORTX+VIEWPORTCX,
                 FSVIEWPORTY+VIEWPORTCY};
    surface->Unlock(&rect);

    // BUT THIS SEEMS TO BE WORKING
    surface->Unlock(NULL);
  }
}

//===========================================================================

// Called when:
// . Mouse f/w sets abs position
// . UpdateMouseInAppleViewport() is called and inside Apple screen
void FrameSetCursorPosByMousePos()
{
	if (!g_hFrameWindow || g_bShowingCursor)
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	sg_Mouse.GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

	_ASSERT(iMinX == 0 && iMinY == 0);
	float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
	float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

	int iWindowX = (int)(fScaleX * (float)VIEWPORTCX);
	int iWindowY = (int)(fScaleY * (float)VIEWPORTCY);

	POINT Point = {viewportx+2, viewporty+2};	// top-left
	ClientToScreen(g_hFrameWindow, &Point);
	SetCursorPos(Point.x+iWindowX-MAGICX, Point.y+iWindowY-MAGICY);

#if defined(_DEBUG) && 0
	static int OldX=0, OldY=0;
	char szDbg[200];
	int X=Point.x+iWindowX-MAGICX;
	int Y=Point.y+iWindowY-MAGICY;
	if (X != OldX || Y != OldY)
	{
		sprintf(szDbg, "[FrameSetCursorPosByMousePos] x,y=%d,%d (MaxX,Y=%d,%d)\n", X,Y, iMaxX,iMaxY); OutputDebugString(szDbg);
		OldX=X; OldY=Y;
	}
#endif
}

static void FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen)
{
//	char szDbg[200];
	if (!g_hFrameWindow || (g_bShowingCursor && bLeavingAppleScreen) || (!g_bShowingCursor && !bLeavingAppleScreen))
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	sg_Mouse.GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);
	_ASSERT(iMinX == 0 && iMinY == 0);

	if (bLeavingAppleScreen)
	{
		// Set mouse x/y pos to edge of mouse's window
		if (dx < 0) iX = iMinX;
		if (dx > 0) iX = iMaxX;
		if (dy < 0) iY = iMinY;
		if (dy > 0) iY = iMaxY;

		float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
		float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

		int iWindowX = (int)(fScaleX * (float)VIEWPORTCX) + dx;
		int iWindowY = (int)(fScaleY * (float)VIEWPORTCY) + dy;

		POINT Point = {viewportx+2, viewporty+2};	// top-left
		ClientToScreen(g_hFrameWindow, &Point);
		SetCursorPos(Point.x+iWindowX-MAGICX, Point.y+iWindowY-MAGICY);
//		sprintf(szDbg, "[MOUSE_LEAVING ] x=%d, y=%d (Scale: x,y=%f,%f; iX,iY=%d,%d)\n", iWindowX, iWindowY, fScaleX, fScaleY, iX, iY); OutputDebugString(szDbg);
	}
	else	// Mouse entering Apple screen area
	{
//		sprintf(szDbg, "[MOUSE_ENTERING] x=%d, y=%d\n", x, y); OutputDebugString(szDbg);
		x -= (viewportx+2-MAGICX);
		y -= (viewporty+2-MAGICY);

		_ASSERT(x <= VIEWPORTCX);
		_ASSERT(y <= VIEWPORTCY);
		float fScaleX = (float)x / (float)VIEWPORTCX;
		float fScaleY = (float)y / (float)VIEWPORTCY;

		int iAppleX = iMinX + (int)(fScaleX * (float)(iMaxX-iMinX));
		int iAppleY = iMinY + (int)(fScaleY * (float)(iMaxY-iMinY));

		sg_Mouse.SetCursorPos(iAppleX, iAppleY);	// Set new entry position

		// Dump initial deltas (otherwise can get big deltas since last read when entering Apple screen area)
		DIMouse::ReadImmediateData();
	}
}

static void DrawCrosshairsMouse()
{
	if (!g_uMouseShowCrosshair)
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	sg_Mouse.GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);
	_ASSERT(iMinX == 0 && iMinY == 0);

	float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
	float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

	int iWindowX = (int)(fScaleX * (float)VIEWPORTCX);
	int iWindowY = (int)(fScaleY * (float)VIEWPORTCY);

	DrawCrosshairs(iWindowX,iWindowY);
}

#ifdef _DEBUG
//#define _DEBUG_SHOW_CURSOR	// NB. Get an ASSERT on LMB (after Ctrl+LMB)
#endif

static void UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x, int y)
{
	const bool bOutsideAppleViewport = iOutOfBoundsX || iOutOfBoundsY;

	if (bOutsideAppleViewport)
	{
		g_bLastCursorInAppleViewport = false;

		if (!g_bShowingCursor)
		{
			// Mouse leaving Apple screen area
			FrameSetCursorPosByMousePos(0, 0, iOutOfBoundsX, iOutOfBoundsY, true);
#ifdef _DEBUG_SHOW_CURSOR
#else
			int nCount = ShowCursor(1);
			_ASSERT(nCount >= 0);
#endif
			g_bShowingCursor = true;
		}
	}
	else
	{
		g_bLastCursorInAppleViewport = true;

		if (g_bShowingCursor)
		{
			// Mouse entering Apple screen area
			FrameSetCursorPosByMousePos(x, y, 0, 0, false);
#ifdef _DEBUG_SHOW_CURSOR
#else
			int nCount = ShowCursor(0);
			_ASSERT(nCount < 0);
#endif
			g_bShowingCursor = false;
		}
		else
		{
			FrameSetCursorPosByMousePos();	// Set cursor to Apple position each time
		}

		DrawCrosshairsMouse();
	}
}
