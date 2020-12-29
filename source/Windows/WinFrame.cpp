/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

#include "Windows/WinFrame.h"
#include "Windows/Win32Frame.h"
#include "Windows/AppleWin.h"
#include "Interface.h"
#include "Keyboard.h"
#include "Log.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "Windows/DirectInput.h"
#include "NTSC.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "SoundCore.h"
#include "Speaker.h"
#include "Utilities.h"
#include "Windows/WinVideo.h"
#include "../resource/resource.h"
#include "Configuration/PropertySheet.h"
#include "Debugger/Debug.h"
#if _MSC_VER < 1900	// VS2013 or before (cl.exe v18.x or before)
#include <sys/stat.h>
#endif

//#define ENABLE_MENU 0
#define DEBUG_KEY_MESSAGES 0

// 3D border around the 560x384 Apple II display
#define  VIEWPORTX   5
#define  VIEWPORTY   5

static const int kDEFAULT_VIEWPORT_SCALE = 2;
       int g_nViewportCX = GetVideo().GetFrameBufferBorderlessWidth()  * kDEFAULT_VIEWPORT_SCALE;
       int g_nViewportCY = GetVideo().GetFrameBufferBorderlessHeight() * kDEFAULT_VIEWPORT_SCALE;
static int g_nViewportScale = kDEFAULT_VIEWPORT_SCALE; // saved REGSAVE
static int g_nMaxViewportScale = kDEFAULT_VIEWPORT_SCALE;	// Max scale in Windowed mode with borders, buttons etc (full-screen may be +1)

#define  BUTTONX     (g_nViewportCX + VIEWPORTX*2)
#define  BUTTONY     0
#define  BUTTONCX    45
#define  BUTTONCY    45
#define  BUTTONS     8

	static HBITMAP g_hCapsLockBitmap[2];
	static HBITMAP g_hHardDiskBitmap[2];

	//Pravets8 only
	static HBITMAP g_hCapsBitmapP8[2];
	static HBITMAP g_hCapsBitmapLat[2];
	//static HBITMAP charsetbitmap [4]; //The idea was to add a charset indicator on the front panel, but it was given up. All charsetbitmap occurences must be REMOVED!
	//===========================
	static HBITMAP g_hDiskWindowedLED[ NUM_DISK_STATUS ];

static int    g_nTrackDrive1  = -1;
static int    g_nTrackDrive2  = -1;
static int    g_nSectorDrive1 = -1;
static int    g_nSectorDrive2 = -1;
static TCHAR  g_sTrackDrive1 [8] = TEXT("??");
static TCHAR  g_sTrackDrive2 [8] = TEXT("??");
static TCHAR  g_sSectorDrive1[8] = TEXT("??");
static TCHAR  g_sSectorDrive2[8] = TEXT("??");
Disk_Status_e g_eStatusDrive1 = DISK_STATUS_OFF;
Disk_Status_e g_eStatusDrive2 = DISK_STATUS_OFF;

// Must keep in sync with Disk_Status_e g_aDiskFullScreenColors
static DWORD g_aDiskFullScreenColorsLED[ NUM_DISK_STATUS ] =
{
	RGB(  0,  0,  0), // DISK_STATUS_OFF   BLACK
	RGB(  0,255,  0), // DISK_STATUS_READ  GREEN
	RGB(255,  0,  0), // DISK_STATUS_WRITE RED
	RGB(255,128,  0)  // DISK_STATUS_PROT  ORANGE
//	RGB(  0,  0,255)  // DISK_STATUS_PROT  -blue-
};

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
static HDC     g_hFrameDC      = (HDC)0;
static RECT    framerect       = {0,0,0,0};

static bool    g_bIsFullScreen  = false;

static BOOL    helpquit        = 0;
static HFONT   smallfont       = (HFONT)0;
static HWND    tooltipwindow   = (HWND)0;
static BOOL    g_bUsingCursor	= FALSE;	// TRUE = AppleWin is using (hiding) the mouse-cursor && restricting cursor to window - see SetUsingCursor()
static int     viewportx       = VIEWPORTX;	// Default to Normal (non-FullScreen) mode
static int     viewporty       = VIEWPORTY;	// Default to Normal (non-FullScreen) mode

static UINT_PTR	g_TimerIDEvent_100msec = 0;
static UINT		g_uCount100msec = 0;

static bool g_bShowingCursor = true;
static bool g_bLastCursorInAppleViewport = false;

static void DrawStatusArea (HDC passdc, int drawflags);
static void ProcessButtonClick (int button, bool bFromButtonUI=false);
void	ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive);
void    RelayEvent (UINT message, WPARAM wparam, LPARAM lparam);
void    SetFullScreenMode ();
void    SetNormalMode ();
static void SetUsingCursor(BOOL);
static bool FileExists(std::string strFilename);

bool	g_bScrollLock_FullSpeed = false;
static bool g_bFullScreen32Bit = true;

#if 0 // enable non-integral full-screen scaling
#define FULLSCREEN_SCALE_TYPE float
#else
#define FULLSCREEN_SCALE_TYPE int
#endif

static RECT						g_main_window_saved_rect;
static int						g_main_window_saved_style;
static int						g_main_window_saved_exstyle;
static FULLSCREEN_SCALE_TYPE	g_win_fullscreen_scale = 1;
static int						g_win_fullscreen_offsetx = 0;
static int						g_win_fullscreen_offsety = 0;

static bool g_bFrameActive = false;
static bool g_windowMinimized = false;

static std::string driveTooltip;

// __ Prototypes __________________________________________________________________________________
void DrawCrosshairs (int x, int y);
void UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x=0, int y=0);
static void ScreenWindowResize(const bool bCtrlKey);
void FrameResizeWindow(int nNewScale);


// ==========================================================================

static bool g_bAltEnter_ToggleFullScreen = true; // Default for ALT+ENTER is to toggle between windowed and full-screen modes

void Win32Frame::SetAltEnterToggleFullScreen(bool mode)
{
	g_bAltEnter_ToggleFullScreen = mode;
}

// ==========================================================================

// Display construction:
// . Apple II video gets rendered to the framebuffer (maybe with some preliminary/final NTSC data in the border areas)
// . The *borderless* framebuffer is stretchblt() copied to the frame DC, in VideoRefreshScreen()
// . Draw cross-hairs (if using mouse as either a mouse or joystick) to frame DC
// . In Windowed mode:
//	 - Draw 3D border, 8x buttons, status area (disk LEDs, caps) to frame DC
// . In Fullscreen mode:
//   - Optional: Draw status area to frame DC
//

UINT Get3DBorderWidth(void)
{
	return IsFullScreen() ? 0 : VIEWPORTX;
}

UINT Get3DBorderHeight(void)
{
	return IsFullScreen() ? 0 : VIEWPORTY;
}

//===========================================================================

static void FrameShowCursor(BOOL bShow)
{
	int nCount;

	if (bShow)
	{
		do
		{
			nCount = ShowCursor(bShow);
		}
		while(nCount < 0);
		g_bShowingCursor = true;
	}
	else
	{
		do
		{
			nCount = ShowCursor(bShow);
		}
		while(nCount >= 0);
		g_bShowingCursor = false;
	}
}

// Called when:
// . Ctrl-Left mouse button
// . PAUSE pressed (when MODE_RUNNING)
// . AppleWin's main window is activated/deactivated
static void RevealCursor()
{
	CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();

	if (!pMouseCard || !pMouseCard->IsActiveAndEnabled())
		return;

	pMouseCard->SetEnabled(false);

	FrameShowCursor(TRUE);

	if (GetPropertySheet().GetMouseShowCrosshair())	// Erase crosshairs if they are being drawn
		DrawCrosshairs(0,0);

	if (GetPropertySheet().GetMouseRestrictToWindow())
		SetUsingCursor(FALSE);

	g_bLastCursorInAppleViewport = false;
}

// Called when:
// . WM_MOUSEMOVE event
// . Switch from full-screen to normal (windowed) mode
// . AppleWin's main window is activated/deactivated
static void FullScreenRevealCursor(void)
{
	if (!g_bIsFullScreen)
		return;

	if (GetCardMgr().IsMouseCardInstalled())
		return;

	if (!g_bUsingCursor && !g_bShowingCursor)
	{
		FrameShowCursor(TRUE);
		g_uCount100msec = 0;
	}
}

//===========================================================================

#define LOADBUTTONBITMAP(bitmapname)  LoadImage(GetFrame().g_hInstance,bitmapname,   \
                                                IMAGE_BITMAP,0,0,      \
                                                LR_CREATEDIBSECTION |  \
                                                LR_LOADMAP3DCOLORS |   \
                                                LR_LOADTRANSPARENT);

static void CreateGdiObjects(void)
{
	memset(buttonbitmap, 0, BUTTONS*sizeof(HBITMAP));

	buttonbitmap[BTN_HELP] = (HBITMAP)LOADBUTTONBITMAP(TEXT("HELP_BUTTON"));

	switch (g_Apple2Type)
	{
	case A2TYPE_PRAVETS82:
	case A2TYPE_PRAVETS8M:
	case A2TYPE_PRAVETS8A:
		buttonbitmap[BTN_RUN] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUNP_BUTTON"));
		break;
	case A2TYPE_TK30002E:
		buttonbitmap[BTN_RUN] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUN3000E_BUTTON"));
		break;
	case A2TYPE_BASE64A:
		buttonbitmap[BTN_RUN] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUNBASE64A_BUTTON"));
		break;
	default:
		buttonbitmap[BTN_RUN] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON"));
		break;
	}

	buttonbitmap[BTN_DRIVE1   ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE1_BUTTON"));
	buttonbitmap[BTN_DRIVE2   ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE2_BUTTON"));
	buttonbitmap[BTN_DRIVESWAP] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVESWAP_BUTTON"));
	buttonbitmap[BTN_FULLSCR  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("FULLSCR_BUTTON"));
	buttonbitmap[BTN_DEBUG    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DEBUG_BUTTON"));
	buttonbitmap[BTN_SETUP    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("SETUP_BUTTON"));

	//

	g_hCapsLockBitmap[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_CAPSOFF_BITMAP"));
	g_hCapsLockBitmap[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_CAPSON_BITMAP"));
	//Pravets8 only
	g_hCapsBitmapP8[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_CAPSOFF_P8_BITMAP"));
	g_hCapsBitmapP8[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_CAPSON_P8_BITMAP"));
	g_hCapsBitmapLat[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_LATOFF_BITMAP"));
	g_hCapsBitmapLat[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LED_LATON_BITMAP"));

	/*charsetbitmap[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CHARSET_APPLE_BITMAP"));
	charsetbitmap[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CHARSET_82_BITMAP"));
	charsetbitmap[2] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CHARSET_8A_BITMAP"));
	charsetbitmap[3] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CHARSET_8M_BITMAP"));
	*/
	//===========================
	g_hDiskWindowedLED[ DISK_STATUS_OFF  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKOFF_BITMAP"));
	g_hDiskWindowedLED[ DISK_STATUS_READ ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKREAD_BITMAP"));
	g_hDiskWindowedLED[ DISK_STATUS_WRITE] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKWRITE_BITMAP"));
	g_hDiskWindowedLED[ DISK_STATUS_PROT ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKPROT_BITMAP"));

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
static void DeleteGdiObjects(void)
{
	for (int loop = 0; loop < BUTTONS; loop++)
		_ASSERT(DeleteObject(buttonbitmap[loop]));

	for (int loop = 0; loop < 2; loop++)
	{
		_ASSERT(DeleteObject(g_hCapsLockBitmap[loop]));
		_ASSERT(DeleteObject(g_hCapsBitmapP8[loop]));
		_ASSERT(DeleteObject(g_hCapsBitmapLat[loop]));
	}

	for (int loop = 0; loop < NUM_DISK_STATUS; loop++)
	{
		_ASSERT(DeleteObject(g_hDiskWindowedLED[loop]));
	}

	_ASSERT(DeleteObject(btnfacebrush));
	_ASSERT(DeleteObject(btnfacepen));
	_ASSERT(DeleteObject(btnhighlightpen));
	_ASSERT(DeleteObject(btnshadowpen));
	_ASSERT(DeleteObject(smallfont));
}

// Draws an 3D box around the main apple screen
//===========================================================================
static void Draw3dRect (HDC dc, int x1, int y1, int x2, int y2, BOOL out)
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
static void DrawBitmapRect (HDC dc, int x, int y, LPRECT rect, HBITMAP bitmap) {
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
static void DrawButton (HDC passdc, int number) {
  FrameReleaseDC();
  HDC dc = (passdc ? passdc : GetDC(GetFrame().g_hFrameWindow));
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

	LPCTSTR pszBaseName = (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		? dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).GetBaseName(number-BTN_DRIVE1).c_str()
		: "";

    ExtTextOut(dc,x+offset+22,rect.top,ETO_CLIPPED,&rect,
               pszBaseName,
               MIN(8,_tcslen(pszBaseName)),
               NULL);
  }
  if (!passdc)
    ReleaseDC(GetFrame().g_hFrameWindow,dc);
}

//===========================================================================

// NB. x=y=0 means erase only
static void DrawCrosshairs (int x, int y) {
  static int lastx = 0;
  static int lasty = 0;
  FrameReleaseDC();
  HDC dc = GetDC(GetFrame().g_hFrameWindow);
#define LINE(x1,y1,x2,y2) MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);

  // ERASE THE OLD CROSSHAIRS
  if (lastx && lasty)
    if (g_bIsFullScreen)
	{
      int loop = 4;
      while (loop--) {
        RECT rect = {0,0,5,5};
        switch (loop) {
          case 0: OffsetRect(&rect, GetFullScreenOffsetX()+lastx-2,                 GetFullScreenOffsetY()+viewporty-5);             break;
          case 1: OffsetRect(&rect, GetFullScreenOffsetX()+lastx-2,                 GetFullScreenOffsetY()+viewporty+g_nViewportCY); break;
          case 2: OffsetRect(&rect, GetFullScreenOffsetX()+viewportx-5,             GetFullScreenOffsetY()+lasty-2);                 break;
          case 3: OffsetRect(&rect, GetFullScreenOffsetX()+viewportx+g_nViewportCX, GetFullScreenOffsetY()+lasty-2);                 break;
        }
        FillRect(dc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
      }
	}
	else
	{
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
        LINE(lastx-2,VIEWPORTY+g_nViewportCY+loop,
             lastx+3,VIEWPORTY+g_nViewportCY+loop);
        LINE(VIEWPORTX+g_nViewportCX+loop,lasty-2,
             VIEWPORTX+g_nViewportCX+loop,lasty+3);
      }
    }

  // DRAW THE NEW CROSSHAIRS
  if (x && y) {
    if (g_bIsFullScreen)
	{
		int loop = 4;
		while (loop--) {
		  if ((loop == 1) || (loop == 2))
			SelectObject(dc,GetStockObject(WHITE_PEN));
		  else
			SelectObject(dc,GetStockObject(BLACK_PEN));
		  LINE(GetFullScreenOffsetX()+x+loop-2,                  GetFullScreenOffsetY()+viewporty-5,
			   GetFullScreenOffsetX()+x+loop-2,                  GetFullScreenOffsetY()+viewporty);
		  LINE(GetFullScreenOffsetX()+x+loop-2,                  GetFullScreenOffsetY()+viewporty+g_nViewportCY+4,
			   GetFullScreenOffsetX()+x+loop-2,                  GetFullScreenOffsetY()+viewporty+g_nViewportCY-1);
		  LINE(GetFullScreenOffsetX()+viewportx-5,               GetFullScreenOffsetY()+y+loop-2,
			   GetFullScreenOffsetX()+viewportx,                 GetFullScreenOffsetY()+y+loop-2);
		  LINE(GetFullScreenOffsetX()+viewportx+g_nViewportCX+4, GetFullScreenOffsetY()+y+loop-2,
			   GetFullScreenOffsetX()+viewportx+g_nViewportCX-1, GetFullScreenOffsetY()+y+loop-2);
		}
	}
	else
	{
		int loop = 4;
		while (loop--) {
		  if ((loop == 1) || (loop == 2))
			SelectObject(dc,GetStockObject(WHITE_PEN));
		  else
			SelectObject(dc,GetStockObject(BLACK_PEN));
		  LINE(x+loop-2,viewporty-5,
			   x+loop-2,viewporty);
		  LINE(x+loop-2,viewporty+g_nViewportCY+4,
			   x+loop-2,viewporty+g_nViewportCY-1);
		  LINE(viewportx-5,           y+loop-2,
			   viewportx,             y+loop-2);
		  LINE(viewportx+g_nViewportCX+4,y+loop-2,
			   viewportx+g_nViewportCX-1,y+loop-2);
		}
	}
  }
#undef LINE
  lastx = x;
  lasty = y;
  ReleaseDC(GetFrame().g_hFrameWindow,dc);
}

//===========================================================================
static void DrawFrameWindow (bool bPaintingWindow = false);
static void DrawFrameWindow (bool bPaintingWindow/*=false*/)
{
	FrameReleaseDC();
	PAINTSTRUCT ps;
	HDC         dc = bPaintingWindow
		? BeginPaint(GetFrame().g_hFrameWindow,&ps)
		: GetDC(GetFrame().g_hFrameWindow);

	if (!g_bIsFullScreen)
	{
		// DRAW THE 3D BORDER AROUND THE EMULATED SCREEN
		Draw3dRect(dc,
			VIEWPORTX-2,VIEWPORTY-2,
			VIEWPORTX+g_nViewportCX+2,VIEWPORTY+g_nViewportCY+2,
			0);
		Draw3dRect(dc,
			VIEWPORTX-3,VIEWPORTY-3,
			VIEWPORTX+g_nViewportCX+3,VIEWPORTY+g_nViewportCY+3,
			0);
		SelectObject(dc,btnfacepen);
		Rectangle(dc,
			VIEWPORTX-4,VIEWPORTY-4,
			VIEWPORTX+g_nViewportCX+4,VIEWPORTY+g_nViewportCY+4);
		Rectangle(dc,
			VIEWPORTX-5,VIEWPORTY-5,
			VIEWPORTX+g_nViewportCX+5,VIEWPORTY+g_nViewportCY+5);

		// DRAW THE TOOLBAR BUTTONS
		int iButton = BUTTONS;
		while (iButton--)
		{
			DrawButton(dc,iButton);
		}

		if (g_nViewportScale == 2)
		{
			int x  = buttonx + 1;
			int y  = buttony + BUTTONS*BUTTONCY + 36;	// 36 = height of StatusArea
			RECT rect = {x, y, x+45, y+BUTTONS*BUTTONCY+22};
			int res = FillRect(dc, &rect, btnfacebrush);
		}
	}

	// DRAW THE STATUS AREA
	DrawStatusArea(dc,DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);

	// DRAW THE CONTENTS OF THE EMULATED SCREEN
	if (g_nAppMode == MODE_LOGO)
		GetVideo().DisplayLogo();
	else if (g_nAppMode == MODE_DEBUG)
		DebugDisplay();
	else
		GetFrame().VideoRedrawScreen();

	if (bPaintingWindow)
		EndPaint(GetFrame().g_hFrameWindow,&ps);
	else
		ReleaseDC(GetFrame().g_hFrameWindow,dc);

}

//===========================================================================
static bool g_bFullScreen_ShowSubunitStatus = true;

bool IsFullScreen(void)
{
	return g_bIsFullScreen;
}

bool GetFullScreenShowSubunitStatus(void)
{
	return g_bFullScreen_ShowSubunitStatus;
}

void Win32Frame::SetFullScreenShowSubunitStatus(bool bShow)
{
	g_bFullScreen_ShowSubunitStatus = bShow;
}

//===========================================================================
void Win32Frame::FrameDrawDiskLEDS( HDC passdc )
{
	g_eStatusDrive1 = DISK_STATUS_OFF;
	g_eStatusDrive2 = DISK_STATUS_OFF;

	// Slot6 drive takes priority unless it's off:
	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).GetLightStatus(&g_eStatusDrive1, &g_eStatusDrive2);

	// Slot5:
	{
		Disk_Status_e eDrive1StatusSlot5 = DISK_STATUS_OFF;
		Disk_Status_e eDrive2StatusSlot5 = DISK_STATUS_OFF;
		if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
			dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT5)).GetLightStatus(&eDrive1StatusSlot5, &eDrive2StatusSlot5);

		if (g_eStatusDrive1 == DISK_STATUS_OFF) g_eStatusDrive1 = eDrive1StatusSlot5;
		if (g_eStatusDrive2 == DISK_STATUS_OFF) g_eStatusDrive2 = eDrive2StatusSlot5;
	}

	// Draw Track/Sector
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(GetFrame().g_hFrameWindow));

	int  x      = buttonx;
	int  y      = buttony+BUTTONS*BUTTONCY+1;

	if (g_bIsFullScreen)
	{
		if (!g_bFullScreen_ShowSubunitStatus)
			return;

		SelectObject(dc,smallfont);
		SetBkMode(dc,OPAQUE);
		SetBkColor(dc,RGB(0,0,0));
		SetTextAlign(dc,TA_LEFT | TA_TOP);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive1] );
		TextOut(dc,x+ 3,y+2,TEXT("1"),1);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive2] );
		TextOut(dc,x+13,y+2,TEXT("2"),1);
	}
	else
	{
		RECT rDiskLed = {0,0,8,8};
		DrawBitmapRect(dc,x+12,y+6,&rDiskLed,g_hDiskWindowedLED[g_eStatusDrive1]);
		DrawBitmapRect(dc,x+31,y+6,&rDiskLed,g_hDiskWindowedLED[g_eStatusDrive2]);
	}
}

// Feature Request #201 Show track status
// https://github.com/AppleWin/AppleWin/issues/201
//===========================================================================
void Win32Frame::FrameDrawDiskStatus( HDC passdc )
{
	if (mem == NULL)
		return;

	if (g_nAppMode == MODE_LOGO)
		return;

	if (g_windowMinimized)	// Prevent DC leaks when app window is minimised (GH#820)
		return;

	// We use the actual drive since probing from memory doesn't tell us anything we don't already know.
	//        DOS3.3   ProDOS
	// Drive  $B7EA    $BE3D
	// Track  $B7EC    LC1 $D356
	// Sector $B7ED    LC1 $D357
	// RWTS            LC1 $D300

	if (GetCardMgr().QuerySlot(SLOT6) != CT_Disk2)
		return;

	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
	int nActiveFloppy = disk2Card.GetCurrentDrive();
	int nDisk1Track = disk2Card.GetTrack(DRIVE_1);
	int nDisk2Track = disk2Card.GetTrack(DRIVE_2);

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
#if _DEBUG && 0
			if (nDOS33track != nDisk1Track)
			{
				char text[128];
				sprintf( text, "\n\n\nWARNING: DOS33Track: %d (%02X) != nDisk1Track: %d (%02X)\n\n\n", nDOS33track, nDOS33track, nDisk1Track, nDisk1Track );
				OutputDebugString( text );
			}
#endif // _DEBUG

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

	sprintf_s( g_sTrackDrive1 , sizeof(g_sTrackDrive1 ), "%2d", g_nTrackDrive1 );
	if (g_nSectorDrive1 < 0) sprintf_s( g_sSectorDrive1, sizeof(g_sSectorDrive1), "??" );
	else                     sprintf_s( g_sSectorDrive1, sizeof(g_sSectorDrive1), "%2d", g_nSectorDrive1 ); 

	sprintf_s( g_sTrackDrive2 , sizeof(g_sTrackDrive2),  "%2d", g_nTrackDrive2 );
	if (g_nSectorDrive2 < 0) sprintf_s( g_sSectorDrive2, sizeof(g_sSectorDrive2), "??" );
	else                     sprintf_s( g_sSectorDrive2, sizeof(g_sSectorDrive2), "%2d", g_nSectorDrive2 );

	// Draw Track/Sector
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(GetFrame().g_hFrameWindow));

	int  x      = buttonx;
	int  y      = buttony+BUTTONS*BUTTONCY+4;

	SelectObject(dc,smallfont);
	SetBkMode(dc,OPAQUE);
	SetBkColor(dc,RGB(0,0,0));
	SetTextAlign(dc,TA_LEFT | TA_TOP);

	char text[ 16 ];

	if (g_bIsFullScreen)
	{
		// GH#57 - drive lights in full screen mode

		if (!g_bFullScreen_ShowSubunitStatus)
			return;

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ g_eStatusDrive1 ] );
		TextOut(dc,x+ 3,y+2,TEXT("1"),1);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ g_eStatusDrive2 ] );
		TextOut(dc,x+13,y+2,TEXT("2"),1);

		int dx = 0;
		if( nActiveFloppy == 0 )
			sprintf( text, "%s/%s    ", g_sTrackDrive1, g_sSectorDrive1 );
		else
			sprintf( text, "%s/%s    ", g_sTrackDrive2, g_sSectorDrive2 );

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ DISK_STATUS_READ ] );
		TextOut(dc,x+dx,y-12,text, strlen(text) ); // original: y+2; y-12 puts status in the Configuration Button Icon
	}
	else
	{
		// NB. Only draw Track/Sector if 2x windowed
		if (g_nViewportScale == 1)
			return;

		// Erase background
		SelectObject(dc,GetStockObject(NULL_PEN));
		SelectObject(dc,btnfacebrush);
		Rectangle(dc,x+4,y+32,x+BUTTONCX+1,y+56); // y+35 -> 44 -> 56

		SetTextColor(dc,RGB(0,0,0));
		SetBkMode(dc,TRANSPARENT);

		sprintf( text, "T%s", g_sTrackDrive1 );
		TextOut(dc,x+6, y+32, text, strlen(text) );
		sprintf( text, "S%s", g_sSectorDrive1 );
		TextOut(dc,x+6, y+42, text, strlen(text) );

		sprintf( text, "T%s", g_sTrackDrive2 );
		TextOut(dc,x+26,y+32, text, strlen(text) );
		sprintf( text, "S%s", g_sSectorDrive2 );
		TextOut(dc,x+26,y+42, text, strlen(text) );
	}
}

//===========================================================================
static void DrawStatusArea (HDC passdc, int drawflags)
{
	if (GetFrame().g_hFrameWindow == NULL)
	{
		// TC: Fix drawing of drive buttons before frame created:
		// . Main init loop: LoadConfiguration() called before FrameCreateWindow(), eg:
		//   LoadConfiguration() -> Disk_LoadLastDiskImage() -> DiskInsert() -> GetFrame().FrameRefreshStatus()
		return;
	}

	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(GetFrame().g_hFrameWindow));
	int  x      = buttonx;
	int  y      = buttony+BUTTONS*BUTTONCY+1;
	const bool bCaps = KeybGetCapsStatus();
	//const bool bP8Caps = KeybGetP8CapsStatus(); // TODO: FIXME: Not used ?!  Should show the LED status ...

#if HD_LED
	// 1.19.0.0 Hard Disk Status/Indicator Light
	Disk_Status_e eHardDriveStatus = DISK_STATUS_OFF;
	HD_GetLightStatus(&eHardDriveStatus);
#endif

	if (g_bIsFullScreen)
	{
		if (!g_bFullScreen_ShowSubunitStatus)
		{
			// Erase Config button icon too, as trk/sec is written here - see FrameDrawDiskStatus()
			RECT rect = {x-2,y-BUTTONCY,x+BUTTONCX+2,y+BUTTONCY};	// Extend rect's width by +/-2 as the TITLE_PAUSED text is wider than an icon button
			FillRect(dc, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
		}
		else
		{
			SelectObject(dc,smallfont);

			if (drawflags & DRAW_DISK_STATUS)
				GetFrame().FrameDrawDiskStatus( dc );

#if HD_LED
			SetTextAlign(dc, TA_RIGHT | TA_TOP);
			SetTextColor(dc, g_aDiskFullScreenColorsLED[ eHardDriveStatus ] );
			TextOut(dc,x+23,y+2,TEXT("H"),1);
#endif

			if (!IS_APPLE2)
			{
				SetTextAlign(dc,TA_RIGHT | TA_TOP);
				SetTextColor(dc,(bCaps
					? RGB(128,128,128)
					: RGB(  0,  0,  0) ));

				TextOut(dc,x+BUTTONCX,y+2,TEXT("A"),1); // NB. Caps Lock indicator is already flush right!
			}

			//

			static const char* pCurrentAppModeText = NULL;

			const char* const pNewAppModeText = (g_nAppMode == MODE_PAUSED)
				? TITLE_PAUSED
				: (g_nAppMode == MODE_STEPPING)
					? TITLE_STEPPING
					: NULL;

			SetTextAlign(dc, TA_CENTER | TA_TOP);

			if (pCurrentAppModeText && pNewAppModeText != pCurrentAppModeText)
			{
				SetTextColor(dc, RGB(0,0,0));
				TextOut(dc, x+BUTTONCX/2, y+13, pCurrentAppModeText, strlen(pCurrentAppModeText));
				pCurrentAppModeText = NULL;
			}

			if (pNewAppModeText)
			{
				SetTextColor(dc, RGB(255,255,255));
				TextOut(dc, x+BUTTONCX/2, y+13, pNewAppModeText, strlen(pNewAppModeText));
				pCurrentAppModeText = pNewAppModeText;
			}
		}
	}
	else // !g_bIsFullScreen
	{
		if (drawflags & DRAW_BACKGROUND)
		{
			SelectObject(dc,GetStockObject(NULL_PEN));
			SelectObject(dc,btnfacebrush);
			Rectangle(dc,x,y,x+BUTTONCX+2,y+34);
			Draw3dRect(dc,x+1,y+3,x+BUTTONCX,y+30,0);

			SelectObject(dc,smallfont);
			SetTextAlign(dc,TA_CENTER | TA_TOP);
			SetTextColor(dc,RGB(0,0,0));
			SetBkMode(dc,TRANSPARENT);
			TextOut(dc,x+ 7,y+5,TEXT("1"),1);
			TextOut(dc,x+27,y+5,TEXT("2"),1);

			// 1.19.0.0 Hard Disk Status/Indicator Light
			TextOut(dc,x+ 7,y+17,TEXT("H"),1);
		}

		if (drawflags & DRAW_LEDS)
		{
			GetFrame().FrameDrawDiskLEDS( dc );

			if (drawflags & DRAW_DISK_STATUS)
				GetFrame().FrameDrawDiskStatus( dc );

			if (!IS_APPLE2)
			{
				RECT rCapsLed = {0,0,10,12}; // HACK: HARD-CODED bitmaps size
				switch (g_Apple2Type)
				{
					case A2TYPE_APPLE2        :			
					case A2TYPE_APPLE2PLUS    :		
					case A2TYPE_APPLE2E       :		
					case A2TYPE_APPLE2EENHANCED:	
					default                   : DrawBitmapRect(dc,x+31,y+17,&rCapsLed,g_hCapsLockBitmap[bCaps != 0]); break;
					case A2TYPE_PRAVETS82     :		
					case A2TYPE_PRAVETS8M     : DrawBitmapRect(dc,x+31,y+17,&rCapsLed,g_hCapsBitmapP8  [bCaps != 0]); break; // TODO: FIXME: Shouldn't one of these use g_hCapsBitmapLat ??
					case A2TYPE_PRAVETS8A     : DrawBitmapRect(dc,x+31,y+17,&rCapsLed,g_hCapsBitmapP8  [bCaps != 0]); break;
				}

#if HD_LED
				// 1.19.0.0 Hard Disk Status/Indicator Light
				RECT rDiskLed = {0,0,8,8};
				DrawBitmapRect(dc,x+12,y+18,&rDiskLed,g_hDiskWindowedLED[eHardDriveStatus]);
#endif
			}
		}

		if (drawflags & DRAW_TITLE)
		{
			GetAppleWindowTitle(); // SetWindowText() // WindowTitle
			SendMessage(GetFrame().g_hFrameWindow,WM_SETTEXT,0,(LPARAM)g_pAppTitle.c_str());
		}

		if (drawflags & DRAW_BUTTON_DRIVES)
		{
			DrawButton(dc, BTN_DRIVE1);
			DrawButton(dc, BTN_DRIVE2);
		}
	}

	if (!passdc)
		ReleaseDC(GetFrame().g_hFrameWindow,dc);
}

//===========================================================================
static void EraseButton (int number) {
  RECT rect;
  rect.left   = buttonx;
  rect.right  = rect.left+BUTTONCX;
  rect.top    = buttony+number*BUTTONCY;
  rect.bottom = rect.top+BUTTONCY;

	InvalidateRect(GetFrame().g_hFrameWindow,&rect,1);
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
    case WM_ACTIVATE:		// Sent when window is activated/deactivated. wParam indicates WA_ACTIVE, WA_INACTIVE, etc
							// Eg. Deactivate when Config dialog is active, AppleWin app loses focus, etc
      JoyReset();
      SetUsingCursor(FALSE);
	  RevealCursor();
	  FullScreenRevealCursor();
	  g_bFrameActive = (wparam != WA_INACTIVE);
      break;

    case WM_ACTIVATEAPP:	// Sent when different app's window is activated/deactivated.
							// Eg. Deactivate when AppleWin app loses focus
      g_bAppActive = (wparam ? TRUE : FALSE);
      break;

	case WM_SIZE:
		switch(wparam)
		{
		case SIZE_RESTORED:
		case SIZE_MAXIMIZED:
			g_windowMinimized = false;
			break;
		case SIZE_MINIMIZED:
			g_windowMinimized = true;
			break;
		default:	// SIZE_MAXSHOW, SIZE_MAXHIDE
			break;
		}
		break;

    case WM_CLOSE:
      LogFileOutput("WM_CLOSE\n");
      if (g_bIsFullScreen && g_bRestart)
		  g_bRestartFullScreen = true;
      if (g_bIsFullScreen)
        SetNormalMode();
      if (!IsIconic(window))
        GetWindowRect(window,&framerect);
      RegSaveValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_X_POS), 1, framerect.left);
      RegSaveValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_Y_POS), 1, framerect.top);
      FrameReleaseDC();
      SetUsingCursor(FALSE);
      if (helpquit) {
        helpquit = 0;
        HtmlHelp(NULL,NULL,HH_CLOSE_ALL,0);
      }
      if (g_TimerIDEvent_100msec)
      {
        BOOL bRes = KillTimer(GetFrame().g_hFrameWindow, g_TimerIDEvent_100msec);
        LogFileOutput("KillTimer(g_TimerIDEvent_100msec), res=%d\n", bRes ? 1 : 0);
        g_TimerIDEvent_100msec = 0;
      }
      LogFileOutput("WM_CLOSE (done)\n");
	  // Exit via DefWindowProc(), which does the default action for WM_CLOSE, which is to call DestroyWindow(), posting WM_DESTROY
      break;

    case WM_DESTROY:
      LogFileOutput("WM_DESTROY\n");
      DragAcceptFiles(window,0);
	  if (!g_bRestart)	// GH#564: Only save-state on shutdown (not on a restart)
		Snapshot_Shutdown();
      DebugDestroy();
      if (!g_bRestart) {
		GetCardMgr().GetDisk2CardMgr().Destroy();
        HD_Destroy();
      }
      PrintDestroy();
      if (GetCardMgr().IsSSCInstalled())
		GetCardMgr().GetSSC()->CommDestroy();
      CpuDestroy();
      MemDestroy();
      SpkrDestroy();
      GetVideo().Destroy();
      MB_Destroy();
      DeleteGdiObjects();
      DIMouse::DirectInputUninit(window);	// NB. do before window is destroyed
      PostQuitMessage(0);	// Post WM_QUIT message to the thread's message queue
      LogFileOutput("WM_DESTROY (done)\n");
      break;

    case WM_CREATE:
      LogFileOutput("WM_CREATE\n");
      GetFrame().g_hFrameWindow = window;	// NB. g_hFrameWindow by CreateWindow()

      CreateGdiObjects();
      LogFileOutput("WM_CREATE: CreateGdiObjects()\n");

	  DSInit();
      LogFileOutput("WM_CREATE: DSInit()\n");

	  DIMouse::DirectInputInit(window);
      LogFileOutput("WM_CREATE: DIMouse::DirectInputInit()\n");

	  MB_Initialize();
      LogFileOutput("WM_CREATE: MB_Initialize()\n");

	  SpkrInitialize();
      LogFileOutput("WM_CREATE: SpkrInitialize()\n");

	  DragAcceptFiles(window,1);
	  LogFileOutput("WM_CREATE: DragAcceptFiles()\n");

      LogFileOutput("WM_CREATE (done)\n");
	  break;

    case WM_DDE_INITIATE: {
      LogFileOutput("WM_DDE_INITIATE\n");
      ATOM application = GlobalAddAtom(TEXT("applewin"));
      ATOM topic       = GlobalAddAtom(TEXT("system"));
      if(LOWORD(lparam) == application && HIWORD(lparam) == topic)
        SendMessage((HWND)wparam,WM_DDE_ACK,(WPARAM)window,MAKELPARAM(application,topic));
      GlobalDeleteAtom(application);
      GlobalDeleteAtom(topic);
      LogFileOutput("WM_DDE_INITIATE (done)\n");
      break;
    }

    case WM_DDE_EXECUTE:
	{
		LogFileOutput("WM_DDE_EXECUTE\n");
		if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		{
			Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			LPTSTR filename = (LPTSTR)GlobalLock((HGLOBAL)lparam);
			ImageError_e Error = disk2Card.InsertDisk(DRIVE_1, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
			if (Error == eIMAGE_ERROR_NONE)
			{
				if (!g_bIsFullScreen)
					DrawButton((HDC)0,BTN_DRIVE1);

				PostMessage(window, WM_USER_BOOT, 0, 0);
			}
			else
			{
				disk2Card.NotifyInvalidImage(DRIVE_1, filename, Error);
			}
		}
		GlobalUnlock((HGLOBAL)lparam);
		LogFileOutput("WM_DDE_EXECUTE (done)\n");
		break;
	}

    case WM_DISPLAYCHANGE:
      GetVideo().VideoReinitialize();
      break;

    case WM_DROPFILES:
	{
		if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		{
			Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
			TCHAR filename[MAX_PATH];
			DragQueryFile((HDROP)wparam,0,filename,sizeof(filename));
			POINT point;
			DragQueryPoint((HDROP)wparam,&point);
			RECT rect;
			rect.left   = buttonx;
			rect.right  = rect.left+BUTTONCX+1;
			rect.top    = buttony+BTN_DRIVE2*BUTTONCY+1;
			rect.bottom = rect.top+BUTTONCY;
			const int iDrive = PtInRect(&rect,point) ? DRIVE_2 : DRIVE_1;
			ImageError_e Error = disk2Card.InsertDisk(iDrive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
			if (Error == eIMAGE_ERROR_NONE)
			{
				if (!g_bIsFullScreen)
					DrawButton((HDC)0,PtInRect(&rect,point) ? BTN_DRIVE2 : BTN_DRIVE1);
				rect.top = buttony+BTN_DRIVE1*BUTTONCY+1;
				if (!PtInRect(&rect,point))
				{
					SetForegroundWindow(window);
					ProcessButtonClick(BTN_RUN);
				}
			}
			else
			{
				disk2Card.NotifyInvalidImage(iDrive, filename, Error);
			}
		}
		DragFinish((HDROP)wparam);
		break;
	}

	// @see: http://answers.google.com/answers/threadview?id=133059
	// Win32 doesn't pass the PrintScreen key via WM_CHAR
	//		else if (wparam == VK_SNAPSHOT)
	// Solution: 2 choices:
	//		1) register hotkey, or
	//		2) Use low level Keyboard hooks
	// We use the 1st one since it is compatible with Win95
	case WM_HOTKEY:
		// wparam = user id
		// lparam = modifiers: shift, ctrl, alt, win
		if (wparam == VK_SNAPSHOT_560)
		{
			GetVideo().Video_TakeScreenShot( Video::SCREENSHOT_560x384 );
		}
		else
		if (wparam == VK_SNAPSHOT_280) // ( lparam & MOD_SHIFT )
		{
			GetVideo().Video_TakeScreenShot( Video::SCREENSHOT_280x192 );
		}
		else
		if (wparam == VK_SNAPSHOT_TEXT) // ( lparam & MOD_CONTROL )
		{
			char  *pText;
			size_t nSize = 0;

			// if viewing the debugger, get the last virtual debugger screen
			if ((g_nAppMode == MODE_DEBUG) && !DebugGetVideoMode(NULL))
				nSize = Util_GetDebuggerText( pText );
			else
				nSize = Util_GetTextScreen( pText );
			Util_CopyTextToClipboard( nSize, pText );
		}
		break;

	case WM_KEYDOWN:
		KeybUpdateCtrlShiftStatus();

		// Processing is done in WM_KEYUP for: VK_F1 VK_F2 VK_F3 VK_F4 VK_F5 VK_F6 VK_F7 VK_F8
		if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == -1))
		{
			SetUsingCursor(FALSE);
			buttondown = wparam-VK_F1;
			if (g_bIsFullScreen && (buttonover != -1)) {
				if (buttonover != buttondown)
				EraseButton(buttonover);
				buttonover = -1;
			}
			DrawButton((HDC)0,buttondown);
		}
		else if (wparam == VK_F9)
		{
			// F9            Next Video Mode
			// SHIFT+F9      Prev Video Mode
			// CTRL+SHIFT+F9 Toggle 50% Scan Lines
			// ALT+F9        Can't use Alt-F9 as Alt is Open-Apple = Joystick Button #1

			if ( !KeybGetCtrlStatus() && !KeybGetShiftStatus() )		// F9
			{
				GetVideo().IncVideoType();
			}
			else if ( !KeybGetCtrlStatus() && KeybGetShiftStatus() )	// SHIFT+F9
			{
				GetVideo().DecVideoType();
			}
			else if ( KeybGetCtrlStatus() && KeybGetShiftStatus() )		// CTRL+SHIFT+F9
			{
				GetVideo().SetVideoStyle( (VideoStyle_e) (GetVideo().GetVideoStyle() ^ VS_HALF_SCANLINES) );
			}

			// TODO: Clean up code:FrameRefreshStatus(DRAW_TITLE) DrawStatusArea((HDC)0,DRAW_TITLE)
			DrawStatusArea( (HDC)0, DRAW_TITLE );

			GetVideo().VideoReinitialize(false);

			if (g_nAppMode != MODE_LOGO)
			{
				if (g_nAppMode == MODE_DEBUG)
				{
					UINT debugVideoMode;
					if ( DebugGetVideoMode(&debugVideoMode) )
						GetVideo().VideoRefreshScreen(debugVideoMode, true);
					else
						GetVideo().VideoPresentScreen();
				}
				else
				{
					GetVideo().VideoPresentScreen();
				}
			}

			GetVideo().Config_Save_Video();
		}
		else if (wparam == VK_F10)
		{
			if (g_Apple2Type == A2TYPE_APPLE2E || g_Apple2Type == A2TYPE_APPLE2EENHANCED || g_Apple2Type == A2TYPE_BASE64A)
			{
				GetVideo().SetVideoRomRockerSwitch( !GetVideo().GetVideoRomRockerSwitch() );	// F10: toggle rocker switch
				NTSC_VideoInitAppleType();
			}
			else if (g_Apple2Type == A2TYPE_PRAVETS8A)
			{
				KeybToggleP8ACapsLock ();	// F10: Toggles Pravets8A Capslock
			}
		}
		else if (wparam == VK_F11 && !KeybGetCtrlStatus())	// Save state (F11)
		{
			SoundCore_SetFade(FADE_OUT);
			if(GetPropertySheet().SaveStateSelectImage(window, true))
			{
				Snapshot_SaveState();
			}
			SoundCore_SetFade(FADE_IN);
		}
		else if (wparam == VK_F12)					// Load state (F12 or Ctrl+F12)
		{
			SoundCore_SetFade(FADE_OUT);
			if(GetPropertySheet().SaveStateSelectImage(window, false))
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
			SetUsingCursor(FALSE);
			switch (g_nAppMode)
			{
				case MODE_RUNNING:
					g_nAppMode = MODE_PAUSED;
					SoundCore_SetFade(FADE_OUT);
					RevealCursor();
					break;
				case MODE_PAUSED:
					g_nAppMode = MODE_RUNNING;
					SoundCore_SetFade(FADE_IN);
					// Don't call FrameShowCursor(FALSE) else ClipCursor() won't be called
					break;
				case MODE_STEPPING:
					SoundCore_SetFade(FADE_OUT);
					DebugStopStepping();
					break;
			}
			DrawStatusArea((HDC)0,DRAW_TITLE);
			if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
				GetFrame().VideoRedrawScreen();
		}
		else if ((wparam == VK_SCROLL) && GetPropertySheet().GetScrollLockToggle())
		{
			g_bScrollLock_FullSpeed = !g_bScrollLock_FullSpeed;
		}
		else if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_LOGO) || (g_nAppMode == MODE_STEPPING))
		{
			// NB. Alt Gr (Right-Alt): this normally send 2 WM_KEYDOWN messages for: VK_LCONTROL, then VK_RMENU
			// . NB. The keyboard hook filter will suppress VK_LCONTROL (if -hook-altgr-control is passed on the cmd-line)
			bool extended = (HIWORD(lparam) & KF_EXTENDED) != 0;
			bool down     = true;
			bool autorep  = (HIWORD(lparam) & KF_REPEAT) != 0;
			BOOL IsJoyKey = JoyProcessKey((int)wparam, extended, down, autorep);

#if DEBUG_KEY_MESSAGES
			LogOutput("WM_KEYDOWN: %08X (scanCode=%04X)\n", wparam, (lparam>>16)&0xfff);
#endif
			if (!IsJoyKey &&
				(g_nAppMode != MODE_LOGO))	// !MODE_LOGO - not emulating so don't pass to the VM's keyboard
			{
				// GH#678 Alternate key(s) to toggle max speed
				// . Ctrl-0: Toggle speed: custom speed / Full-Speed
				// . Ctrl-1: Speed = 1 MHz
				// . Ctrl-3: Speed = Full-Speed
				bool keyHandled = false;
				if( KeybGetCtrlStatus() &&
					!KeybGetAltStatus() &&		// GH#749 - AltGr also fakes CTRL being pressed!
					wparam >= '0' && wparam <= '9' )
				{
					switch (wparam)
					{
						case '0':	// Toggle speed: custom speed / Full-Speed
							if (g_dwSpeed == SPEED_MAX)
								REGLOAD_DEFAULT(TEXT(REGVALUE_EMULATION_SPEED), &g_dwSpeed, SPEED_NORMAL);
							else
								g_dwSpeed = SPEED_MAX;
							keyHandled = true; break;
						case '1':	// Speed = 1 MHz
							g_dwSpeed = SPEED_NORMAL;
							REGSAVE(TEXT(REGVALUE_EMULATION_SPEED), g_dwSpeed);
							keyHandled = true; break;
						case '3':	// Speed = Full-Speed
							g_dwSpeed = SPEED_MAX;
							keyHandled = true; break;
						default:
							break;
					}

					if (keyHandled)
						SetCurrentCLK6502();
				}

				if (!keyHandled)
					KeybQueueKeypress(wparam, NOT_ASCII);

				if (!autorep)
					KeybAnyKeyDown(WM_KEYDOWN, wparam, extended);
			}
		}
		else if (g_nAppMode == MODE_DEBUG)
		{		
			DebuggerProcessKey(wparam); // Debugger already active, re-direct key to debugger
		}
		break;

		case WM_CHAR:
			if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING) || (g_nAppMode == MODE_LOGO))
			{
				if (!g_bDebuggerEatKey)
				{
#if DEBUG_KEY_MESSAGES
					LogOutput("WM_CHAR: %08X\n", wparam);
#endif
					if (g_nAppMode != MODE_LOGO)	// !MODE_LOGO - not emulating so don't pass to the VM's keyboard
						KeybQueueKeypress(wparam, ASCII);
				}

				g_bDebuggerEatKey = false;
			}
			else if (g_nAppMode == MODE_DEBUG)
			{
				DebuggerInputConsoleChar((TCHAR)wparam);
			}
			break;

	case WM_KEYUP:
		if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == (int)wparam-VK_F1))
		{
			buttondown = -1;
			if (g_bIsFullScreen)
				EraseButton(wparam-VK_F1);
			else
				DrawButton((HDC)0,wparam-VK_F1);

			const int iButton = wparam-VK_F1;
			if (KeybGetCtrlStatus() && (wparam == VK_F3 || wparam == VK_F4))	// Ctrl+F3/F4 for drive pop-up menu (GH#817)
			{
				POINT pt;		// location of mouse click
				pt.x = buttonx + BUTTONCX/2;
				pt.y = buttony + BUTTONCY/2 + iButton * BUTTONCY;
				const int iDrive = wparam - VK_F3;
				ProcessDiskPopupMenu( window, pt, iDrive );

				GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
				DrawButton((HDC)0, iButton);
			}
			else
			{
				ProcessButtonClick(iButton, true);
			}
		}
		else
		{
			bool extended = (HIWORD(lparam) & KF_EXTENDED) != 0;
			bool down     = false;
			bool autorep  = false;
			BOOL bIsJoyKey = JoyProcessKey((int)wparam, extended, down, autorep);

#if DEBUG_KEY_MESSAGES
			LogOutput("WM_KEYUP: %08X\n", wparam);
#endif
			if (!bIsJoyKey)
				KeybAnyKeyDown(WM_KEYUP, wparam, extended);
		}
		break;

    case WM_LBUTTONDOWN:
		KeybUpdateCtrlShiftStatus();

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
        else if (g_bUsingCursor && !GetCardMgr().IsMouseCardInstalled())
		{
          if (wparam & (MK_CONTROL | MK_SHIFT))
		  {
            SetUsingCursor(FALSE);
		  }
          else
		  {
	        JoySetButton(BUTTON0, BUTTON_DOWN);
		  }
		}
        else if ( ((x < buttonx) && JoyUsingMouse() && ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))) )
		{
          SetUsingCursor(TRUE);
		}
		else if (GetCardMgr().IsMouseCardInstalled())
		{
			if (wparam & (MK_CONTROL | MK_SHIFT))
			{
				RevealCursor();
			}
			else if (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_STEPPING)
			{
				CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();

				if (pMouseCard)
				{
					if (!pMouseCard->IsEnabled())
					{
						pMouseCard->SetEnabled(true);

						POINT Point;
						GetCursorPos(&Point);
						ScreenToClient(GetFrame().g_hFrameWindow, &Point);
						const int iOutOfBoundsX=0, iOutOfBoundsY=0;
						UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY, Point.x, Point.y);

						// Don't call SetButton() when 1st enabled (else get the confusing action of both enabling & an Apple mouse click)
					}
					else
					{
						pMouseCard->SetButton(BUTTON0, BUTTON_DOWN);
					}
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
          if (g_bIsFullScreen)
            EraseButton(buttonactive);
          else
            DrawButton((HDC)0,buttonactive);
          ProcessButtonClick(buttonactive, true);
        }
        buttonactive = -1;
      }
      else if (g_bUsingCursor && !GetCardMgr().IsMouseCardInstalled())
	  {
	    JoySetButton(BUTTON0, BUTTON_UP);
	  }
	  else if (GetCardMgr().IsMouseCardInstalled())
	  {
		GetCardMgr().GetMouseCard()->SetButton(BUTTON0, BUTTON_UP);
	  }
      RelayEvent(WM_LBUTTONUP,wparam,lparam);
      break;

    case WM_MOUSEMOVE: {
      // MSDN: "WM_MOUSEMOVE message" : Do not use the LOWORD or HIWORD macros to extract the x- and y- coordinates...
      int x = GET_X_LPARAM(lparam);
      int y = GET_Y_LPARAM(lparam);
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
      else if (g_bIsFullScreen && (newover != buttonover) && (buttondown == -1)) {
        if (buttonover != -1)
          EraseButton(buttonover);
        buttonover = newover;
        if (buttonover != -1)
          DrawButton((HDC)0,buttonover);
      }
      else if (g_bUsingCursor && !GetCardMgr().IsMouseCardInstalled())
	  {
        DrawCrosshairs(x,y);
	    JoySetPosition(x-viewportx-2, g_nViewportCX-4, y-viewporty-2, g_nViewportCY-4);
      }
	  else if (GetCardMgr().IsMouseCardInstalled() && GetCardMgr().GetMouseCard()->IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_STEPPING))
	  {
			if (g_bLastCursorInAppleViewport)
				break;

			// Outside Apple viewport

			const int iAppleScreenMaxX = g_nViewportCX-1;
			const int iAppleScreenMaxY = g_nViewportCY-1;
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

      FullScreenRevealCursor();

      RelayEvent(WM_MOUSEMOVE,wparam,lparam);
      break;
    }

	case WM_TIMER:
		if (wparam == IDEVENT_TIMER_MOUSE)
		{
			// NB. Need to check /g_bAppActive/ since WM_TIMER events still occur after AppleWin app has lost focus
			if (g_bAppActive && GetCardMgr().IsMouseCardInstalled() && GetCardMgr().GetMouseCard()->IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_STEPPING))
			{
				if (!g_bLastCursorInAppleViewport)
					break;

				// Inside Apple viewport

				int iOutOfBoundsX=0, iOutOfBoundsY=0;

				long dX,dY;
				if (DIMouse::ReadImmediateData(&dX, &dY) == S_OK)
					GetCardMgr().GetMouseCard()->SetPositionRel(dX, dY, &iOutOfBoundsX, &iOutOfBoundsY);

				UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY);
			}
		}
		else if (wparam == IDEVENT_TIMER_100MSEC)	// GH#504
		{
			if (g_bIsFullScreen
				&& !GetCardMgr().IsMouseCardInstalled()	// Don't interfere if there's a mousecard present!
				&& !g_bUsingCursor			// Using mouse for joystick emulation (or mousecard restricted to window)
				&& g_bShowingCursor
				&& g_bFrameActive)			// Frame inactive when eg. Config or 'Select Disk Image' dialogs are opened
			{
				g_uCount100msec++;
				if (g_uCount100msec > 20)	// Hide every 2sec of mouse inactivity
				{
					FrameShowCursor(FALSE);
				}
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

    case WM_NOTIFY:	// Tooltips for Drive buttons
		if (((LPNMTTDISPINFO)lparam)->hdr.hwndFrom == tooltipwindow && ((LPNMTTDISPINFO)lparam)->hdr.code == TTN_GETDISPINFO)
		{
			LPNMTTDISPINFO pInfo = (LPNMTTDISPINFO)lparam;
			SendMessage(pInfo->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 150);

			Disk2InterfaceCard *pDisk2Slot5 = NULL, *pDisk2Slot6 = NULL;

			if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
				pDisk2Slot5 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT5));
			if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
				pDisk2Slot6 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT6));

			std::string slot5 = pDisk2Slot5 ? pDisk2Slot5->GetFullDiskFilename(((LPNMTTDISPINFO)lparam)->hdr.idFrom) : "";
			std::string slot6 = pDisk2Slot6 ? pDisk2Slot6->GetFullDiskFilename(((LPNMTTDISPINFO)lparam)->hdr.idFrom) : "";

			if (pDisk2Slot5)
			{
				if (slot6.empty()) slot6 = "<empty>";
				if (slot5.empty()) slot5 = "<empty>";
				slot6 = std::string("Slot6: ") + slot6;
				slot5 = std::string("Slot5: ") + slot5;
			}

			std::string join = (!slot6.empty() && !slot5.empty()) ? "\r\n" : "";
			driveTooltip = slot6 + join + slot5;
			((LPNMTTDISPINFO)lparam)->lpszText = (LPTSTR)driveTooltip.c_str();
		}
		break;

    case WM_PAINT:
      if (GetUpdateRect(window,NULL,0)){
        DrawFrameWindow(true);
      }
      break;

    case WM_PALETTECHANGED:
		// To avoid creating an infinite loop, a window that receives this
		// message must not realize its palette, unless it determines that
		// wParam does not contain its own window handle.
		if ((HWND)wparam == window)
			break;
	  // else fall through

    case WM_QUERYNEWPALETTE:
      DrawFrameWindow();
      break;

    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
		if ((buttonover == -1) && (message == WM_RBUTTONUP)) // HACK: BUTTON_NONE
		{
			int x = LOWORD(lparam);
			int y = HIWORD(lparam);

			if ((x >= buttonx) &&
				(y >= buttony) &&
				(y <= buttony+BUTTONS*BUTTONCY))
			{
				const int iButton = (y-buttony-1)/BUTTONCY;
				const int iDrive = iButton - BTN_DRIVE1;
				if ((iButton == BTN_DRIVE1) || (iButton == BTN_DRIVE2))
				{
					{
						RECT  rect;		// client area
						POINT pt;		// location of mouse click

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

					GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
					DrawButton((HDC)0, iButton);
				}			
			}
		}

		if (g_bUsingCursor && !GetCardMgr().IsMouseCardInstalled())
			JoySetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);
		else if (GetCardMgr().IsMouseCardInstalled())
			GetCardMgr().GetMouseCard()->SetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);

		RelayEvent(message,wparam,lparam);
		break;

    case WM_SYSCOLORCHANGE:
#if DEBUG_DD_PALETTE
		if( g_bIsFullScreen )
			OutputDebugString( "WM_SYSCOLORCHANGE: Full Screen\n" );
		else
			OutputDebugString( "WM_SYSCOLORCHANGE: Windowed\n" );
#endif

		DeleteGdiObjects();
		CreateGdiObjects();
		break;

    case WM_SYSCOMMAND:
      switch (wparam & 0xFFF0) {
        case SC_KEYMENU:
          if (g_bIsFullScreen && g_bAppActive)
            return 0;
          break;
        case SC_MINIMIZE:
          GetWindowRect(window,&framerect);
          break;
      }
      break;

	case WM_SYSKEYDOWN:	// ALT + any key; or F10
		KeybUpdateCtrlShiftStatus();

		// http://msdn.microsoft.com/en-us/library/windows/desktop/gg153546(v=vs.85).aspx
		if (g_bAltEnter_ToggleFullScreen && KeybGetAltStatus() && (wparam == VK_RETURN)) // NB. VK_RETURN = 0x0D; Normally WM_CHAR will be 0x0A but ALT key triggers as WM_SYSKEYDOWN and VK_MENU
			return 0; // NOP -- eat key

		PostMessage(window,WM_KEYDOWN,wparam,lparam);

		if ((wparam == VK_F10) || (wparam == VK_MENU))	// VK_MENU == ALT Key
			return 0;

		break;

	case WM_SYSKEYUP:
		KeybUpdateCtrlShiftStatus();

		// F10: no WM_KEYUP handler for VK_F10. Don't allow WM_KEYUP to pass to default handler which will show the app window's "menu" (and lose focus)
		if (wparam == VK_F10)
			return 0;

		if (g_bAltEnter_ToggleFullScreen && KeybGetAltStatus() && (wparam == VK_RETURN)) // NB. VK_RETURN = 0x0D; Normally WM_CHAR will be 0x0A but ALT key triggers as WM_SYSKEYDOWN and VK_MENU
			ScreenWindowResize(false);
		else
			PostMessage(window,WM_KEYUP,wparam,lparam);

		break;

	case WM_MENUCHAR:	// GH#556 - Suppress the Windows Default Beep (ie. Ding) whenever ALT+<key> is pressed
		return (MNC_CLOSE << 16) | (wparam & 0xffff);

    case WM_USER_BENCHMARK: {
      UpdateWindow(window);
      ResetMachineState();
      DrawStatusArea((HDC)0,DRAW_TITLE);
      HCURSOR oldcursor = SetCursor(LoadCursor(0,IDC_WAIT));
      g_nAppMode = MODE_BENCHMARK;
      GetVideo().Benchmark();
      g_nAppMode = MODE_LOGO;
      ResetMachineState();
      SetCursor(oldcursor);
      break;
    }

    case WM_USER_RESTART:
	  // Changed h/w config, eg. Apple computer type (][+ or //e), slot configuration, etc.
      g_bRestart = true;
      PostMessage(window,WM_CLOSE,0,0);
      break;

    case WM_USER_SAVESTATE:		// Save state
		Snapshot_SaveState();
		break;

    case WM_USER_LOADSTATE:		// Load state
		Snapshot_LoadState();
		break;

	case WM_USER_TCP_SERIAL:	// TCP serial events
	{
		WORD error = WSAGETSELECTERROR(lparam);
		if (error != 0)
		{
			LogOutput("TCP Serial Winsock error 0x%X (%d)\r", error, error);
			switch (error)
			{
			case WSAENETRESET:
			case WSAECONNABORTED:
			case WSAECONNRESET:
			case WSAENOTCONN:
			case WSAETIMEDOUT:
				if (GetCardMgr().IsSSCInstalled())
					GetCardMgr().GetSSC()->CommTcpSerialClose();
				break;

			default:
				if (GetCardMgr().IsSSCInstalled())
					GetCardMgr().GetSSC()->CommTcpSerialCleanup();
				break;
			}
		}
		else
		{
			WORD wSelectEvent = WSAGETSELECTEVENT(lparam);
			switch(wSelectEvent)
			{
				case FD_ACCEPT:
					if (GetCardMgr().IsSSCInstalled())
						GetCardMgr().GetSSC()->CommTcpSerialAccept();
					break;

				case FD_CLOSE:
					if (GetCardMgr().IsSSCInstalled())
						GetCardMgr().GetSSC()->CommTcpSerialClose();
					break;

				case FD_READ:
					if (GetCardMgr().IsSSCInstalled())
						GetCardMgr().GetSSC()->CommTcpSerialReceive();
					break;
			}
		}
		break;
	}

	// Message posted by: WM_DDE_EXECUTE & Cmd-line boot
	case WM_USER_BOOT:
	{
		SetForegroundWindow(window);
		Sleep(500);	// Wait for SetForegroundWindow() to take affect (400ms seems OK, so use 500ms to be sure)
		SoundCore_TweakVolumes();
		ProcessButtonClick(BTN_RUN);
		break;
	}

	// Message posted by: Cmd-line boot
	case WM_USER_FULLSCREEN:
	{
		ScreenWindowResize(false);
		break;
	}

  }	// switch(message)
 
  return DefWindowProc(window,message,wparam,lparam);
}



//===========================================================================
// Process: VK_F6
static void ScreenWindowResize(const bool bCtrlKey)
{
	static int nOldViewportScale = kDEFAULT_VIEWPORT_SCALE;

	if (g_bIsFullScreen)	// if full screen: then switch back to normal
	{
		SetNormalMode();
		FrameResizeWindow(nOldViewportScale);
	}
	else if (bCtrlKey)		// if normal screen && CTRL: then toggle scaling
	{
		FrameResizeWindow( (g_nViewportScale == 1) ? 2 : 1 );	// Toggle between 1x and 2x
		REGSAVE(TEXT(REGVALUE_WINDOW_SCALE), g_nViewportScale);
	}
	else
	{
		nOldViewportScale = g_nViewportScale;
		FrameResizeWindow(1);	// reset to 1x
		SetFullScreenMode();
	}
}

static bool ConfirmReboot(bool bFromButtonUI)
{
	if (!bFromButtonUI || !GetFrame().g_bConfirmReboot)
		return true;

	int res = MessageBox(GetFrame().g_hFrameWindow, 
		"Are you sure you want to reboot?\n"
		"(All data will be lost!)\n"
		"\n"
		"You can skip this dialog from displaying\n"
		"in the future by unchecking:\n"
		"\n"
		"    [ ] Confirm reboot\n"
		"\n"
		"in the Configuration dialog.\n"
		, "Reboot", MB_ICONWARNING|MB_YESNO);
	return res == IDYES;
}

static void ProcessButtonClick(int button, bool bFromButtonUI /*=false*/)
{
	SoundCore_SetFade(FADE_OUT);
	bool bAllowFadeIn = true;

#if DEBUG_DD_PALETTE
	char _text[ 80 ];
	sprintf( _text, "Button: F%d  Full Screen: %d\n", button+1, g_bIsFullScreen );
	OutputDebugString( _text );
#endif

  switch (button) {

    case BTN_HELP:
      {
        const std::string filename = g_sProgramDir + TEXT("APPLEWIN.CHM");

		// (GH#437) For any internet downloaded AppleWin.chm files (stored on an NTFS drive) there may be an Alt Data Stream containing a Zone Identifier
		// - try to delete it, otherwise the content won't be displayed unless it's unblock (via File Properties)
		{
			const std::string filename_with_zone_identifier = filename + TEXT(":Zone.Identifier");
			DeleteFile(filename_with_zone_identifier.c_str());
		}

        HtmlHelp(GetFrame().g_hFrameWindow,filename.c_str(),HH_DISPLAY_TOC,0);
        helpquit = 1;
      }
      break;

    case BTN_RUN:
		KeybUpdateCtrlShiftStatus();
		if( KeybGetCtrlStatus() )
		{
			CtrlReset();
			if (g_nAppMode == MODE_DEBUG)
				DebugDisplay(TRUE);
			return;
		}

		if (g_nAppMode == MODE_LOGO)
		{
			if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
				dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).Boot();

			LogFileTimeUntilFirstKeyReadReset();
			g_nAppMode = MODE_RUNNING;
		}
		else if ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING) || (g_nAppMode == MODE_PAUSED))
		{
			if (ConfirmReboot(bFromButtonUI))
			{
				ResetMachineState();

				// NB. Don't exit debugger or stepping

				if (g_nAppMode == MODE_DEBUG)
					DebugDisplay(TRUE);
			}
		}

      DrawStatusArea((HDC)0,DRAW_TITLE);
      GetFrame().VideoRedrawScreen();
      break;

    case BTN_DRIVE1:
    case BTN_DRIVE2:
		if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).UserSelectNewDiskImage(button-BTN_DRIVE1);
			if (!g_bIsFullScreen)
				DrawButton((HDC)0,button);
		}
		break;

    case BTN_DRIVESWAP:
		if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).DriveSwap();
		}
		break;

    case BTN_FULLSCR:
		KeybUpdateCtrlShiftStatus();
		ScreenWindowResize( KeybGetCtrlStatus() );
      break;

    case BTN_DEBUG:
		if (g_nAppMode == MODE_LOGO && !GetLoadedSaveStateFlag())
		{
			// FIXME: Why is this needed? Surely when state is MODE_LOGO, then AppleII system will have been reset!
			// - Transition to MODE_LOGO when: (a) AppleWin starts, (b) there's a Config change to the AppleII h/w
			ResetMachineState();
		}

		if (g_nAppMode == MODE_STEPPING)
		{
			// Allow F7 to enter debugger even when not MODE_RUNNING
			DebugStopStepping();
			bAllowFadeIn = false;
		}
		else if (g_nAppMode == MODE_DEBUG)
		{
			DebugExitDebugger(); // Exit debugger, switch to MODE_RUNNING or MODE_STEPPING
			g_bDebuggerEatKey = false;	// Don't "eat" the next keypress when leaving the debugger via F7 (or clicking the Debugger button)
		}
		else	// MODE_RUNNING, MODE_LOGO, MODE_PAUSED
		{
			DebugBegin();
		}
      break;

    case BTN_SETUP:
      {
		  GetPropertySheet().Init();
      }
      break;

  }

  if((g_nAppMode != MODE_DEBUG) && (g_nAppMode != MODE_PAUSED) && bAllowFadeIn)
  {
	  SoundCore_SetFade(FADE_IN);
  }
}


//===========================================================================

// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/winui/windowsuserinterface/resources/menus/usingmenus.asp
// http://www.codeproject.com/menu/MenusForBeginners.asp?df=100&forumid=67645&exp=0&select=903061

void ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive)
{
	if (GetCardMgr().QuerySlot(SLOT6) != CT_Disk2)
		return;

	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));

	// This is the default installation path of CiderPress. 
	// It shall not be left blank, otherwise  an explorer window will be open.
	TCHAR PathToCiderPress[MAX_PATH];
	RegLoadString(
		TEXT("Configuration"),
		REGVALUE_CIDERPRESSLOC,
		1,
		PathToCiderPress,
		MAX_PATH,
		TEXT("C:\\Program Files\\faddenSoft\\CiderPress\\CiderPress.exe"));
	//TODO: A directory is open if an empty path to CiderPress is set. This has to be fixed.

	std::string filename1= "\"";
	filename1.append( disk2Card.GetFullName(iDrive) );
	filename1.append("\"");
	std::string sFileNameEmpty = "\"";
	sFileNameEmpty.append("\"");

	//  Load the menu template containing the shortcut menu from the
	//  application's resources.
	HMENU hmenu = LoadMenu(GetFrame().g_hInstance, MAKEINTRESOURCE(IDR_MENU_DISK_POPUP));	// menu template
	if (hmenu == NULL)
		return;

	// Get the first shortcut menu in the menu template.
	// This is the menu that TrackPopupMenu displays.
	HMENU hmenuTrackPopup = GetSubMenu(hmenu, 0);	// shortcut menu

	// TrackPopup uses screen coordinates, so convert the
	// coordinates of the mouse click to screen coordinates.
	ClientToScreen(hwnd, (LPPOINT) &pt);

	// Check menu depending on current floppy protection
	{
		int iMenuItem = ID_DISKMENU_WRITEPROTECTION_OFF;
		if (disk2Card.GetProtect( iDrive ))
			iMenuItem = ID_DISKMENU_WRITEPROTECTION_ON;

		CheckMenuItem(hmenu, iMenuItem, MF_CHECKED);
	}

	if (disk2Card.IsDriveEmpty(iDrive))
		EnableMenuItem(hmenu, ID_DISKMENU_EJECT, MF_GRAYED);

	if (disk2Card.IsDiskImageWriteProtected(iDrive))
	{
		// If image-file is read-only (or a gzip) then disable these menu items
		EnableMenuItem(hmenu, ID_DISKMENU_WRITEPROTECTION_ON, MF_GRAYED);
		EnableMenuItem(hmenu, ID_DISKMENU_WRITEPROTECTION_OFF, MF_GRAYED);
	}

	// Draw and track the shortcut menu.
	int iCommand = TrackPopupMenu(
		hmenuTrackPopup
		, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD
		, pt.x, pt.y
		, 0
		, hwnd, NULL );

	if (iCommand == ID_DISKMENU_EJECT)
		disk2Card.EjectDisk( iDrive );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_ON)
		disk2Card.SetProtect( iDrive, true );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_OFF)
		disk2Card.SetProtect( iDrive, false );
	else
	if (iCommand == ID_DISKMENU_SENDTO_CIDERPRESS)
	{
		static char szCiderpressNotFoundCaption[] = "CiderPress not found";
		static char szCiderpressNotFoundText[] =	"CiderPress not found!\n"
													"Please install CiderPress.\n"
													"Otherwise set the path to CiderPress from Configuration->Disk.";

		disk2Card.FlushCurrentTrack(iDrive);

		//if(!filename1.compare("\"\"") == false) //Do not use this, for some reason it does not work!!!
		if(!filename1.compare(sFileNameEmpty) )
		{
			int MB_Result = MessageBox(GetFrame().g_hFrameWindow, "No disk image loaded. Do you want to run CiderPress anyway?" ,"No disk image.", MB_ICONINFORMATION|MB_YESNO);
			if (MB_Result == IDYES)
			{
				if (FileExists (PathToCiderPress ))
				{
					HINSTANCE nResult  = ShellExecute(NULL, "open", PathToCiderPress, "" , NULL, SW_SHOWNORMAL);
				}
				else
				{
					MessageBox(GetFrame().g_hFrameWindow, szCiderpressNotFoundText, szCiderpressNotFoundCaption, MB_ICONINFORMATION|MB_OK);
				}
			}
		}
		else
		{
			if (FileExists (PathToCiderPress ))
			{
				HINSTANCE nResult  = ShellExecute(NULL, "open", PathToCiderPress, filename1.c_str() , NULL, SW_SHOWNORMAL);
			}
			else
			{
				MessageBox(GetFrame().g_hFrameWindow, szCiderpressNotFoundText, szCiderpressNotFoundCaption, MB_ICONINFORMATION|MB_OK);
			}
		}
	}

	// Destroy the menu.
	BOOL bRes = DestroyMenu(hmenu);
	_ASSERT(bRes);
}


//===========================================================================
void RelayEvent (UINT message, WPARAM wparam, LPARAM lparam) {
  if (g_bIsFullScreen)
    return;
  MSG msg;
  msg.hwnd    = GetFrame().g_hFrameWindow;
  msg.message = message;
  msg.wParam  = wparam;
  msg.lParam  = lparam;
  SendMessage(tooltipwindow,TTM_RELAYEVENT,0,(LPARAM)&msg);
}


//===========================================================================

int GetFullScreenOffsetX(void)
{
	return g_win_fullscreen_offsetx;
}

int GetFullScreenOffsetY(void)
{
	return g_win_fullscreen_offsety;
}

void SetFullScreenMode ()
{
#ifdef NO_DIRECT_X

	return;

#else // NO_DIRECT_X

	MONITORINFO monitor_info;
	FULLSCREEN_SCALE_TYPE width, height, scalex, scaley;
	int top, left;

	buttonover = -1;

	g_main_window_saved_style = GetWindowLong(GetFrame().g_hFrameWindow, GWL_STYLE);
	g_main_window_saved_exstyle = GetWindowLong(GetFrame().g_hFrameWindow, GWL_EXSTYLE);
	GetWindowRect(GetFrame().g_hFrameWindow, &g_main_window_saved_rect);
	SetWindowLong(GetFrame().g_hFrameWindow, GWL_STYLE  , g_main_window_saved_style   & ~(WS_CAPTION | WS_THICKFRAME));
	SetWindowLong(GetFrame().g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
	
	monitor_info.cbSize = sizeof(monitor_info);
	GetMonitorInfo(MonitorFromWindow(GetFrame().g_hFrameWindow, MONITOR_DEFAULTTONEAREST), &monitor_info);

	left = monitor_info.rcMonitor.left;
	top  = monitor_info.rcMonitor.top;

	width  = (FULLSCREEN_SCALE_TYPE)(monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left);
	height = (FULLSCREEN_SCALE_TYPE)(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top );

	scalex = width  / GetVideo().GetFrameBufferBorderlessWidth();
	scaley = height / GetVideo().GetFrameBufferBorderlessHeight();

	g_win_fullscreen_scale = (scalex <= scaley) ? scalex : scaley;
	g_win_fullscreen_offsetx = ((int)width  - (int)(g_win_fullscreen_scale * GetVideo().GetFrameBufferBorderlessWidth())) / 2;
	g_win_fullscreen_offsety = ((int)height - (int)(g_win_fullscreen_scale * GetVideo().GetFrameBufferBorderlessHeight())) / 2;
	SetWindowPos(GetFrame().g_hFrameWindow, NULL, left, top, (int)width, (int)height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	g_bIsFullScreen = true;

	GetFrame().SetViewportScale(g_win_fullscreen_scale, true);

	buttonx    = GetFullScreenOffsetX() + g_nViewportCX + VIEWPORTX*2;
	buttony    = GetFullScreenOffsetY();
	viewportx  = VIEWPORTX;		// TC-TODO: Should be zero too? (Since there's no 3D border in full-screen)
	viewporty  = 0; // GH#464

	InvalidateRect(GetFrame().g_hFrameWindow,NULL,1);

#endif // NO_DIRECT_X
}

//===========================================================================
void SetNormalMode ()
{
	FullScreenRevealCursor();	// Do before clearing g_bIsFullScreen flag

	buttonover = -1;
	buttonx    = BUTTONX;
	buttony    = BUTTONY;
	viewportx  = VIEWPORTX;
	viewporty  = VIEWPORTY;

	g_win_fullscreen_offsetx = 0;
	g_win_fullscreen_offsety = 0;
	g_win_fullscreen_scale = 1;
	SetWindowLong(GetFrame().g_hFrameWindow, GWL_STYLE, g_main_window_saved_style);
	SetWindowLong(GetFrame().g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle);
	SetWindowPos(GetFrame().g_hFrameWindow, NULL,
		g_main_window_saved_rect.left,
		g_main_window_saved_rect.top,
		g_main_window_saved_rect.right - g_main_window_saved_rect.left,
		g_main_window_saved_rect.bottom - g_main_window_saved_rect.top,
		SWP_SHOWWINDOW);
	g_bIsFullScreen = false;
}

//===========================================================================
static void SetUsingCursor (BOOL bNewValue)
{
	if (bNewValue == g_bUsingCursor)
		return;

	g_bUsingCursor = bNewValue;

	if (g_bUsingCursor)
	{
		// Set TRUE when:
		// . Using mouse for joystick emulation
		// . Using mousecard and mouse is restricted to window
		SetCapture(GetFrame().g_hFrameWindow);
		RECT rect =	{	viewportx+2,				// left
						viewporty+2,				// top
						viewportx+g_nViewportCX-1,	// right
						viewporty+g_nViewportCY-1};	// bottom
		ClientToScreen(GetFrame().g_hFrameWindow,(LPPOINT)&rect.left);
		ClientToScreen(GetFrame().g_hFrameWindow,(LPPOINT)&rect.right);
		ClipCursor(&rect);
		FrameShowCursor(FALSE);
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(GetFrame().g_hFrameWindow,&pt);
		DrawCrosshairs(pt.x,pt.y);
	}
	else
	{
		DrawCrosshairs(0,0);
		FrameShowCursor(TRUE);
		ClipCursor(NULL);
		ReleaseCapture();
	}
}

int GetViewportScale(void)
{
	return g_nViewportScale;
}

int Win32Frame::SetViewportScale(int nNewScale, bool bForce /*=false*/)
{
	if (!bForce && nNewScale > g_nMaxViewportScale)
		nNewScale = g_nMaxViewportScale;

	g_nViewportScale = nNewScale;
	g_nViewportCX = g_nViewportScale * GetVideo().GetFrameBufferBorderlessWidth();
	g_nViewportCY = g_nViewportScale * GetVideo().GetFrameBufferBorderlessHeight();

	return nNewScale;
}

static void SetupTooltipControls(void)
{
	TOOLINFO toolinfo;
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.uFlags = TTF_CENTERTIP;
	toolinfo.hwnd = GetFrame().g_hFrameWindow;
	toolinfo.hinst = GetFrame().g_hInstance;
	toolinfo.lpszText = LPSTR_TEXTCALLBACK;
	toolinfo.rect.left  = BUTTONX;
	toolinfo.rect.right = toolinfo.rect.left+BUTTONCX+1;
	toolinfo.uId = 0;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE1*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
	toolinfo.uId = 1;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE2*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
}

// SM_CXPADDEDBORDER is not supported on 2000 & XP, but GetSystemMetrics() returns 0 for unknown values, so this use of SM_CXPADDEDBORDER works on 2000 & XP too:
// http://msdn.microsoft.com/en-nz/library/windows/desktop/ms724385(v=vs.85).aspx
// NB. GetSystemMetrics(SM_CXPADDEDBORDER) returns 0 for Win7, when built with VS2008 (see GH#571)
static void GetWidthHeight(int& nWidth, int& nHeight)
{
	nWidth  = g_nViewportCX + VIEWPORTX*2
						    + BUTTONCX
						    + (GetSystemMetrics(SM_CXFIXEDFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)) * 2;
	nHeight = g_nViewportCY + VIEWPORTY*2
						    + (GetSystemMetrics(SM_CYFIXEDFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)) * 2	// NB. No SM_CYPADDEDBORDER
						    + GetSystemMetrics(SM_CYCAPTION);

#if 0	// GH#571
	LogOutput("g_nViewportCX                       = %d\n", g_nViewportCX);
	LogOutput("VIEWPORTX                           = %d (const)\n", VIEWPORTX);
	LogOutput("BUTTONCX                            = %d (const)\n", BUTTONCX);
	LogOutput("GetSystemMetrics(SM_CXFRAME)        = %d (unused)\n", GetSystemMetrics(SM_CXFRAME));
	LogOutput("GetSystemMetrics(SM_CXFIXEDFRAME)   = %d\n", GetSystemMetrics(SM_CXFIXEDFRAME));
	LogOutput("GetSystemMetrics(SM_CXBORDER)       = %d (unused)\n", GetSystemMetrics(SM_CXBORDER));
	LogOutput("GetSystemMetrics(SM_CXPADDEDBORDER) = %d\n", GetSystemMetrics(SM_CXPADDEDBORDER));
	LogOutput("nWidth                              = %d\n", nWidth);
	LogOutput("g_nViewportCY                       = %d\n", g_nViewportCY);
	LogOutput("VIEWPORTY                           = %d (const)\n", VIEWPORTY);
	LogOutput("GetSystemMetrics(SM_CYFRAME)        = %d (unused)\n", GetSystemMetrics(SM_CYFRAME));
	LogOutput("GetSystemMetrics(SM_CYFIXEDFRAME)   = %d\n", GetSystemMetrics(SM_CYFIXEDFRAME));
	LogOutput("GetSystemMetrics(SM_CYBORDER)       = %d (unused)\n", GetSystemMetrics(SM_CYBORDER));
	LogOutput("GetSystemMetrics(SM_CYCAPTION)      = %d\n", GetSystemMetrics(SM_CYCAPTION));
	LogOutput("nHeight                             = %d\n\n", nHeight);
#endif
}

static void FrameResizeWindow(int nNewScale)
{
	int nOldWidth, nOldHeight;
	GetWidthHeight(nOldWidth, nOldHeight);

	nNewScale = GetFrame().SetViewportScale(nNewScale);

	GetWindowRect(GetFrame().g_hFrameWindow, &framerect);
	int nXPos = framerect.left;
	int nYPos = framerect.top;

	//

	buttonx = g_nViewportCX + VIEWPORTX*2;
	buttony = 0;

	// Invalidate old rect region
	{
		RECT irect;
		irect.left = irect.top = 0;
		irect.right = nOldWidth;
		irect.bottom = nOldHeight;
		InvalidateRect(GetFrame().g_hFrameWindow, &irect, TRUE);
	}

	// Resize the window
	int nNewWidth, nNewHeight;
	GetWidthHeight(nNewWidth, nNewHeight);

	MoveWindow(GetFrame().g_hFrameWindow, nXPos, nYPos, nNewWidth, nNewHeight, TRUE);
	UpdateWindow(GetFrame().g_hFrameWindow);

	// Remove the tooltips for the old window size
	TOOLINFO toolinfo = {0};
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.hwnd = GetFrame().g_hFrameWindow;
	toolinfo.uId = 0;
	SendMessage(tooltipwindow, TTM_DELTOOL, 0, (LPARAM)&toolinfo);
	toolinfo.uId = 1;
	SendMessage(tooltipwindow, TTM_DELTOOL, 0, (LPARAM)&toolinfo);

	// Setup the tooltips for the new window size
	SetupTooltipControls();
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void FrameCreateWindow(void)
{
	int nWidth, nHeight;

	// Set g_nMaxViewportScale
	{
		int nOldViewportCX = g_nViewportCX;
		int nOldViewportCY = g_nViewportCY;

		g_nViewportCX = GetVideo().GetFrameBufferBorderlessWidth() * 2;
		g_nViewportCY = GetVideo().GetFrameBufferBorderlessHeight() * 2;
		GetWidthHeight(nWidth, nHeight);	// Probe with 2x dimensions

		g_nViewportCX = nOldViewportCX;
		g_nViewportCY = nOldViewportCY;

		if (nWidth > GetSystemMetrics(SM_CXSCREEN) || nHeight > GetSystemMetrics(SM_CYSCREEN))
			g_nMaxViewportScale = 1;
	}

	GetWidthHeight(nWidth, nHeight);

	// If screen is too small for 2x, then revert to 1x
	if (g_nViewportScale == 2 && (nWidth > GetSystemMetrics(SM_CXSCREEN) || nHeight > GetSystemMetrics(SM_CYSCREEN)))
	{
		g_nMaxViewportScale = 1;
		GetFrame().SetViewportScale(1);
		GetWidthHeight(nWidth, nHeight);
	}

	// Restore Window X Position
	int nXPos = -1;
	{
		const int nXScreen = GetSystemMetrics(SM_CXSCREEN) - nWidth;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_X_POS), 1, (DWORD*)&nXPos))
		{
			if ((nXPos > nXScreen) && !GetFrame().g_bMultiMon)
				nXPos = -1;	// Not fully visible, so default to centre position
		}

		if ((nXPos == -1) && !GetFrame().g_bMultiMon)
			nXPos = nXScreen / 2;
	}

	// Restore Window Y Position
	int nYPos = -1;
	{
		const int nYScreen = GetSystemMetrics(SM_CYSCREEN) - nHeight;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_Y_POS), 1, (DWORD*)&nYPos))
		{
			if ((nYPos > nYScreen) && !GetFrame().g_bMultiMon)
				nYPos = -1;	// Not fully visible, so default to centre position
		}

		if ((nYPos == -1) && !GetFrame().g_bMultiMon)
			nYPos = nYScreen / 2;
	}

	//

	buttonx = (g_nViewportCX + VIEWPORTX*2);
	buttony = 0;

	GetAppleWindowTitle();

	// NB. g_hFrameWindow also set by WM_CREATE - NB. CreateWindow() must synchronously send WM_CREATE
	GetFrame().g_hFrameWindow = CreateWindow(
		TEXT("APPLE2FRAME"),
		g_pAppTitle.c_str(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_VISIBLE,
		nXPos, nYPos, nWidth, nHeight,
		HWND_DESKTOP,
		(HMENU)0,
		GetFrame().g_hInstance, NULL );

	InitCommonControls();
	tooltipwindow = CreateWindow(
		TOOLTIPS_CLASS,NULL,TTS_ALWAYSTIP, 
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, 
		GetFrame().g_hFrameWindow,
		(HMENU)0,
		GetFrame().g_hInstance,NULL );

	SetupTooltipControls();

	_ASSERT(g_TimerIDEvent_100msec == 0);
	g_TimerIDEvent_100msec = SetTimer(GetFrame().g_hFrameWindow, IDEVENT_TIMER_100MSEC, 100, NULL);
	LogFileOutput("FrameCreateWindow: SetTimer(), id=0x%08X\n", g_TimerIDEvent_100msec);
}

//===========================================================================
HDC FrameGetDC () {
  if (!g_hFrameDC) {
    g_hFrameDC = GetDC(GetFrame().g_hFrameWindow);
    SetViewportOrgEx(g_hFrameDC,viewportx,viewporty,NULL);
  }
  return g_hFrameDC;
}

//===========================================================================
void FrameReleaseDC () {
  if (g_hFrameDC) {
    SetViewportOrgEx(g_hFrameDC,0,0,NULL);
    ReleaseDC(GetFrame().g_hFrameWindow,g_hFrameDC);
    g_hFrameDC = (HDC)0;
  }
}

//===========================================================================
void Win32Frame::FrameRefreshStatus (int drawflags, bool bUpdateDiskStatus) {
	// NB. 99% of the time we draw the disk status.  On DiskDriveSwap() we don't.
 	drawflags |= bUpdateDiskStatus ? DRAW_DISK_STATUS : 0;
	DrawStatusArea((HDC)0,drawflags);
}

//===========================================================================
void FrameRegisterClass () {
  WNDCLASSEX wndclass;
  memset(&wndclass, 0, sizeof(WNDCLASSEX));
  wndclass.cbSize        = sizeof(WNDCLASSEX);
  wndclass.style         = CS_OWNDC | CS_BYTEALIGNCLIENT;
  wndclass.lpfnWndProc   = FrameWndProc;
  wndclass.hInstance     = GetFrame().g_hInstance;
  wndclass.hIcon         = LoadIcon(GetFrame().g_hInstance,TEXT("APPLEWIN_ICON"));
  wndclass.hCursor       = LoadCursor(0,IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
#if ENABLE_MENU
  wndclass.lpszMenuName	 = (LPCSTR)IDR_MENU1;
#endif
  wndclass.lpszClassName = TEXT("APPLE2FRAME");
  wndclass.hIconSm       = (HICON)LoadImage(GetFrame().g_hInstance,TEXT("APPLEWIN_ICON"),
                                            IMAGE_ICON,16,16,LR_DEFAULTCOLOR);
  RegisterClassEx(&wndclass);
}

//===========================================================================
// TODO: FIXME: Util_TestFileExists()
static bool FileExists(std::string strFilename) 
{
	struct _stat64 stFileInfo; 
	int intStat = _stat64(strFilename.c_str(), &stFileInfo);
	return (intStat == 0);
}

//===========================================================================

// Called when:
// . Mouse f/w sets abs position
// . UpdateMouseInAppleViewport() is called and inside Apple screen
void Win32Frame::FrameSetCursorPosByMousePos()
{
//	_ASSERT(GetCardMgr().IsMouseCardInstalled());	// CMouseInterface::ctor calls this function, ie. before GetCardMgr()::m_pMouseCard is setup
	if (!GetCardMgr().IsMouseCardInstalled())
		return;

	if (!GetFrame().g_hFrameWindow || g_bShowingCursor)
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	GetCardMgr().GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

	float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
	float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

	int iWindowX = (int)(fScaleX * (float)g_nViewportCX);
	int iWindowY = (int)(fScaleY * (float)g_nViewportCY);

	POINT Point = {viewportx+2, viewporty+2};	// top-left
	ClientToScreen(GetFrame().g_hFrameWindow, &Point);
	SetCursorPos(Point.x+iWindowX-VIEWPORTX, Point.y+iWindowY-VIEWPORTY);

#if defined(_DEBUG) && 0	// OutputDebugString() when cursor position changes since last time
	static int OldX=0, OldY=0;
	char szDbg[200];
	int X=Point.x+iWindowX-VIEWPORTX;
	int Y=Point.y+iWindowY-VIEWPORTY;
	if (X != OldX || Y != OldY)
	{
		sprintf(szDbg, "[FrameSetCursorPosByMousePos] x,y=%d,%d (MaxX,Y=%d,%d)\n", X,Y, iMaxX,iMaxY); OutputDebugString(szDbg);
		OldX=X; OldY=Y;
	}
#endif
}

// Called when:
// . UpdateMouseInAppleViewport() is called and mouse leaving/entering Apple screen area
// . NB. Not called when leaving & mouse clipped to Apple screen area
static void FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen)
{
	_ASSERT(GetCardMgr().IsMouseCardInstalled());
	if (!GetCardMgr().IsMouseCardInstalled())
		return;

//	char szDbg[200];
	if (!GetFrame().g_hFrameWindow || (g_bShowingCursor && bLeavingAppleScreen) || (!g_bShowingCursor && !bLeavingAppleScreen))
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	GetCardMgr().GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

	if (bLeavingAppleScreen)
	{
		// Set mouse x/y pos to edge of mouse's window
		if (dx < 0) iX = iMinX;
		if (dx > 0) iX = iMaxX;
		if (dy < 0) iY = iMinY;
		if (dy > 0) iY = iMaxY;

		float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
		float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

		int iWindowX = (int)(fScaleX * (float)g_nViewportCX) + dx;
		int iWindowY = (int)(fScaleY * (float)g_nViewportCY) + dy;

		POINT Point = {viewportx+2, viewporty+2};	// top-left
		ClientToScreen(GetFrame().g_hFrameWindow, &Point);
		SetCursorPos(Point.x+iWindowX-VIEWPORTX, Point.y+iWindowY-VIEWPORTY);
//		sprintf(szDbg, "[MOUSE_LEAVING ] x=%d, y=%d (Scale: x,y=%f,%f; iX,iY=%d,%d)\n", iWindowX, iWindowY, fScaleX, fScaleY, iX, iY); OutputDebugString(szDbg);
	}
	else	// Mouse entering Apple screen area
	{
//		sprintf(szDbg, "[MOUSE_ENTERING] x=%d, y=%d\n", x, y); OutputDebugString(szDbg);
		if (!g_bIsFullScreen)	// GH#464
		{
			x -= (viewportx+2-VIEWPORTX); if (x < 0) x = 0;
			y -= (viewporty+2-VIEWPORTY); if (y < 0) y = 0;
		}

		_ASSERT(x <= g_nViewportCX);
		_ASSERT(y <= g_nViewportCY);
		float fScaleX = (float)x / (float)g_nViewportCX;
		float fScaleY = (float)y / (float)g_nViewportCY;

		int iAppleX = iMinX + (int)(fScaleX * (float)(iMaxX-iMinX));
		int iAppleY = iMinY + (int)(fScaleY * (float)(iMaxY-iMinY));

		GetCardMgr().GetMouseCard()->SetCursorPos(iAppleX, iAppleY);	// Set new entry position

		// Dump initial deltas (otherwise can get big deltas since last read when entering Apple screen area)
		DIMouse::ReadImmediateData();
	}
}

static void DrawCrosshairsMouse()
{
	_ASSERT(GetCardMgr().IsMouseCardInstalled());
	if (!GetCardMgr().IsMouseCardInstalled())
		return;

	if (!GetPropertySheet().GetMouseShowCrosshair())
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	GetCardMgr().GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);
	_ASSERT(iMinX == 0 && iMinY == 0);

	float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
	float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

	int iWindowX = (int)(fScaleX * (float)g_nViewportCX);
	int iWindowY = (int)(fScaleY * (float)g_nViewportCY);

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
		if (GetPropertySheet().GetMouseRestrictToWindow())
			return;

		g_bLastCursorInAppleViewport = false;

		if (!g_bShowingCursor)
		{
			// Mouse leaving Apple screen area
			FrameSetCursorPosByMousePos(0, 0, iOutOfBoundsX, iOutOfBoundsY, true);
#ifdef _DEBUG_SHOW_CURSOR
			g_bShowingCursor = true;
#else
			FrameShowCursor(TRUE);
#endif
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
			g_bShowingCursor = false;
#else
			FrameShowCursor(FALSE);
#endif

			//

			if (GetPropertySheet().GetMouseRestrictToWindow())
				SetUsingCursor(TRUE);
		}
		else
		{
			GetFrame().FrameSetCursorPosByMousePos();	// Set cursor to Apple position each time
		}

		DrawCrosshairsMouse();
	}
}

void GetViewportCXCY(int& nViewportCX, int& nViewportCY)
{
	nViewportCX = g_nViewportCX;
	nViewportCY = g_nViewportCY;
}

// Call all funcs with dependency on g_Apple2Type
void Win32Frame::FrameUpdateApple2Type(void)
{
	DeleteGdiObjects();
	CreateGdiObjects();

	// DRAW_TITLE : calls GetAppleWindowTitle()
	// DRAW_LEDS  : update LEDs (eg. CapsLock varies on Apple2 type)
	DrawStatusArea( (HDC)0, DRAW_TITLE|DRAW_LEDS );

	// Draw buttons & call DrawStatusArea(DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS)
	DrawFrameWindow();
}

bool Win32Frame::GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedHeight /*= 0*/)
{
	typedef std::vector< std::pair<UINT,UINT> > VEC_PAIR;
	VEC_PAIR vecDisplayResolutions;

	for (UINT iModeNum = 0; ; iModeNum++)
	{
		DEVMODE devMode;
		devMode.dmSize = sizeof(DEVMODE);
		devMode.dmDriverExtra = 0;
		BOOL bValid = EnumDisplaySettings(NULL, iModeNum, &devMode);
		if (!bValid)
			break;
		if (iModeNum == 0)	// 0 is the initial "cache info about display device" operation
			continue;

		if (devMode.dmBitsPerPel != 32)
			continue;

		if (userSpecifiedHeight == 0 || userSpecifiedHeight == devMode.dmPelsHeight)
		{
			if (vecDisplayResolutions.size() == 0 || vecDisplayResolutions.back() != std::pair<UINT,UINT>(devMode.dmPelsWidth, devMode.dmPelsHeight)	)	// Skip duplicate resolutions
			{
				vecDisplayResolutions.push_back( std::pair<UINT,UINT>(devMode.dmPelsWidth, devMode.dmPelsHeight) );
				LogFileOutput("EnumDisplaySettings(%d) - %d x %d\n", iModeNum, devMode.dmPelsWidth, devMode.dmPelsHeight);
			}
		}
	}

	if (userSpecifiedHeight)
	{
		if (vecDisplayResolutions.size() == 0)
			return false;

		// Pick least width (such that it's wide enough to scale)
		UINT width = (UINT)-1;
		for (VEC_PAIR::iterator it = vecDisplayResolutions.begin(); it!= vecDisplayResolutions.end(); ++it)
		{
			if (width > it->first)
			{
				UINT scaleFactor = it->second / GetVideo().GetFrameBufferBorderlessHeight();
				if (it->first >= (GetVideo().GetFrameBufferBorderlessWidth() * scaleFactor))
				{
					width = it->first;
				}
			}
		}

		if (width == (UINT)-1)
			return false;

		bestWidth = width;
		bestHeight = userSpecifiedHeight;
		return true;
	}

	// Pick max height that's an exact multiple of GetFrameBufferBorderlessHeight()
	UINT tmpBestWidth = 0;
	UINT tmpBestHeight = 0;
	for (VEC_PAIR::iterator it = vecDisplayResolutions.begin(); it!= vecDisplayResolutions.end(); ++it)
	{
		if ((it->second % GetVideo().GetFrameBufferBorderlessHeight()) == 0)
		{
			if (it->second > tmpBestHeight)
			{
				UINT scaleFactor = it->second / GetVideo().GetFrameBufferBorderlessHeight();
				if (it->first >= (GetVideo().GetFrameBufferBorderlessWidth() * scaleFactor))
				{
					tmpBestWidth = it->first;
					tmpBestHeight = it->second;
				}
			}
		}
	}

	if (tmpBestWidth == 0)
		return false;

	bestWidth = tmpBestWidth;
	bestHeight = tmpBestHeight;
	return true;
}
