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
#include "DiskImage.h"
#include "Harddisk.h"
#pragma  hdrstop
#include "MouseInterface.h"
#include "..\resource\resource.h"
#include <sys/stat.h>

//#define ENABLE_MENU 0

// Magic numbers (used by FrameCreateWindow to calc width/height):
#define MAGICX 5	// 3D border between Apple window & Emulator's RHS buttons
#define MAGICY 5	// 3D border between Apple window & Title bar

#define VIEWPORTCX FRAMEBUFFER_W
#define VIEWPORTCY FRAMEBUFFER_H

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
//Pravets8 only
static HBITMAP capsbitmapP8[2];
static HBITMAP latbitmap[2];
//static HBITMAP charsetbitmap [4]; //The idea was to add a charset indicator on the front panel, but it was given up. All charsetbitmap occurences must be REMOVED!
//===========================
static HBITMAP g_hDiskWindowedLED[ NUM_DISK_STATUS ];

//static HBITMAP g_hDiskFullScreenLED[ NUM_DISK_STATUS ];

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
static HRGN    clipregion      = (HRGN)0;
       HDC     g_hFrameDC         = (HDC)0;
static RECT    framerect       = {0,0,0,0};

		HWND   g_hFrameWindow  = (HWND)0;
		BOOL   g_bIsFullScreen = 0;

static BOOL    helpquit        = 0;
static BOOL    g_bPaintingWindow        = 0;
static HFONT   smallfont       = (HFONT)0;
static HWND    tooltipwindow   = (HWND)0;
static BOOL    g_bUsingCursor	= 0;		// 1=AppleWin is using (hiding) the mouse-cursor
static int     viewportx       = VIEWPORTX;	// Default to Normal (non-FullScreen) mode
static int     viewporty       = VIEWPORTY;	// Default to Normal (non-FullScreen) mode
int g_nCharsetType = 0;

// Direct Draw -- For Full Screen
		LPDIRECTDRAW        g_pDD = (LPDIRECTDRAW)0;
		LPDIRECTDRAWSURFACE g_pDDPrimarySurface    = (LPDIRECTDRAWSURFACE)0;
		IDirectDrawPalette* g_pDDPal = (IDirectDrawPalette*)0;


static bool g_bShowingCursor = true;
static bool g_bLastCursorInAppleViewport = false;

void    DrawStatusArea (HDC passdc, BOOL drawflags);
void    ProcessButtonClick (int button);
void	ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive);
void    RelayEvent (UINT message, WPARAM wparam, LPARAM lparam);
void    ResetMachineState ();
void    SetFullScreenMode ();
void    SetNormalMode ();
void    SetUsingCursor (BOOL);
static bool FileExists(string strFilename);

bool	g_bScrollLock_FullSpeed = false;
bool	g_bFreshReset = false;

// Prototypes:
static void DrawCrosshairs (int x, int y);
static void FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen);
static void DrawCrosshairsMouse();
static void UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x=0, int y=0);

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
// . AppleWin's main window is deactivated
static void RevealCursor()
{
	if (!sg_Mouse.IsActiveAndEnabled())
		return;

	sg_Mouse.SetEnabled(false);

	FrameShowCursor(TRUE);

	if (g_uMouseShowCrosshair)	// Erase crosshairs if they are being drawn
		DrawCrosshairs(0,0);

	if (g_uMouseRestrictToWindow)
		SetUsingCursor(FALSE);

	g_bLastCursorInAppleViewport = false;
}

//===========================================================================

static void CreateGdiObjects () {
  ZeroMemory(buttonbitmap,BUTTONS*sizeof(HBITMAP));
#define LOADBUTTONBITMAP(bitmapname)  LoadImage(g_hInstance,bitmapname,   \
                                                IMAGE_BITMAP,0,0,      \
                                                LR_CREATEDIBSECTION |  \
                                                LR_LOADMAP3DCOLORS |   \
                                                LR_LOADTRANSPARENT);
  buttonbitmap[BTN_HELP   ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("HELP_BUTTON"));
  buttonbitmap[BTN_RUN    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON"));
switch (g_Apple2Type)
			{
			case A2TYPE_APPLE2:			buttonbitmap[BTN_RUN    ] =(HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON")); break; 
			case A2TYPE_APPLE2PLUS:		buttonbitmap[BTN_RUN    ] =(HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON")); break; 
			case A2TYPE_APPLE2E:		buttonbitmap[BTN_RUN    ] =(HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON")); break; 
			case A2TYPE_APPLE2EEHANCED:	buttonbitmap[BTN_RUN    ] =(HBITMAP)LOADBUTTONBITMAP(TEXT("RUN_BUTTON")); break; 
			case A2TYPE_PRAVETS82:		buttonbitmap[BTN_RUN    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUNP_BUTTON")); break; 
			case A2TYPE_PRAVETS8M:		buttonbitmap[BTN_RUN    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUNP_BUTTON")); break; 
			case A2TYPE_PRAVETS8A:		buttonbitmap[BTN_RUN    ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("RUNP_BUTTON")); break; 
			}

  buttonbitmap[BTN_DRIVE1 ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE1_BUTTON"));
  buttonbitmap[BTN_DRIVE2 ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVE2_BUTTON"));
  buttonbitmap[BTN_DRIVESWAP] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DRIVESWAP_BUTTON"));
  buttonbitmap[BTN_FULLSCR] = (HBITMAP)LOADBUTTONBITMAP(TEXT("FULLSCR_BUTTON"));
  buttonbitmap[BTN_DEBUG  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DEBUG_BUTTON"));
  buttonbitmap[BTN_SETUP  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("SETUP_BUTTON"));
  buttonbitmap[BTN_P8CAPS ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSON_BITMAP"));
  capsbitmap[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSOFF_BITMAP"));
  capsbitmap[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSON_BITMAP"));
  //Pravets8 only
  capsbitmapP8[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSOFF_P8_BITMAP"));
  capsbitmapP8[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("CAPSON_P8_BITMAP"));
  latbitmap[0] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LATOFF_BITMAP"));
  latbitmap[1] = (HBITMAP)LOADBUTTONBITMAP(TEXT("LATON_BITMAP"));
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
  
  // Full Screen Drive LED
//	g_hDiskFullScreenLED[ DISK_STATUS_OFF  ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISK_FULLSCREEN_O")); // Full Screen Off
//	g_hDiskFullScreenLED[ DISK_STATUS_READ ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISK_FULLSCREEN_R")); // Full Screen Read Only
//	g_hDiskFullScreenLED[ DISK_STATUS_WRITE] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISK_FULLSCREEN_W")); // Full Screen Write
//	g_hDiskFullScreenLED[ DISK_STATUS_PROT ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISK_FULLSCREEN_P")); // Full Screen Write Protected
 
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
static void DeleteGdiObjects () {
  int loop;
  for (loop = 0; loop < BUTTONS; loop++)
    DeleteObject(buttonbitmap[loop]);
  for (loop = 0; loop < 2; loop++)
    DeleteObject(capsbitmap[loop]);
  for (loop = 0; loop < NUM_DISK_STATUS; loop++)
  {
	  DeleteObject(g_hDiskWindowedLED[loop]);
	  //DeleteObject(g_hDiskFullScreenLED[loop]);
  }
  DeleteObject(btnfacebrush);
  DeleteObject(btnfacepen);
  DeleteObject(btnhighlightpen);
  DeleteObject(btnshadowpen);
  DeleteObject(smallfont);
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
	LPCTSTR pszBaseName = DiskGetBaseName(number-BTN_DRIVE1);
    ExtTextOut(dc,x+offset+22,rect.top,ETO_CLIPPED,&rect,
               pszBaseName,
               MIN(8,_tcslen(pszBaseName)),
               NULL);
  }
  if (!passdc)
    ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================
static void DrawCrosshairs (int x, int y) {
  static int lastx = 0;
  static int lasty = 0;
  FrameReleaseDC();
  HDC dc = GetDC(g_hFrameWindow);
#define LINE(x1,y1,x2,y2) MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);

  // ERASE THE OLD CROSSHAIRS
  if (lastx && lasty)
    if (g_bIsFullScreen) {
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
static void DrawFrameWindow ()
{
	FrameReleaseDC();
	PAINTSTRUCT ps;
	HDC         dc = (g_bPaintingWindow
		? BeginPaint(g_hFrameWindow,&ps)
		: GetDC(g_hFrameWindow));

	VideoRealizePalette(dc);

	if (!g_bIsFullScreen)
	{
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
		int iButton = BUTTONS;
		while (iButton--)
		{
			DrawButton(dc,iButton);
		}
	}

	// DRAW THE STATUS AREA
	DrawStatusArea(dc,DRAW_BACKGROUND | DRAW_LEDS);

	// DRAW THE CONTENTS OF THE EMULATED SCREEN
	if (g_nAppMode == MODE_LOGO)
		VideoDisplayLogo();
	else if (g_nAppMode == MODE_DEBUG)
		DebugDisplay(1);
	else
		VideoRedrawScreen();

	// DD Full-Screen Palette: BUGFIX: needs to come _after_ all drawing...
	if (g_bPaintingWindow)
		EndPaint(g_hFrameWindow,&ps);
	else
		ReleaseDC(g_hFrameWindow,dc);

}

//===========================================================================
static void DrawStatusArea (HDC passdc, int drawflags)
{
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(g_hFrameWindow));
	int  x      = buttonx;
	int  y      = buttony+BUTTONS*BUTTONCY+1;
	int  iDrive1Status = DISK_STATUS_OFF;
	int  iDrive2Status = DISK_STATUS_OFF;
	bool bCaps   = KeybGetCapsStatus();
	bool bP8Caps  = KeybGetP8CapsStatus();
	DiskGetLightStatus(&iDrive1Status,&iDrive2Status);

	if (g_bIsFullScreen)
	{
		SelectObject(dc,smallfont);
		SetBkMode(dc,OPAQUE);
		SetBkColor(dc,RGB(0,0,0));
		SetTextAlign(dc,TA_LEFT | TA_TOP);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ iDrive1Status ] );
		TextOut(dc,x+ 3,y+2,TEXT("1"),1);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ iDrive2Status ] );
		TextOut(dc,x+13,y+2,TEXT("2"),1);

		// Feature Request #3581 ] drive lights in full screen mode
		// This has been in for a while, at least since 1.12.7.1

		// Full Screen Drive LED
		// Note: Made redundant with above code
		//		RECT rect = {0,0,8,8};
		//		CONST int DriveLedY = 12; // 8 in windowed mode
		//		DrawBitmapRect(dc,x+12,y+DriveLedY,&rect,g_hDiskFullScreenLED[ iDrive1Status ]);
		//		DrawBitmapRect(dc,x+30,y+DriveLedY,&rect,g_hDiskFullScreenLED[ iDrive2Status ]);
		//		SetTextColor(dc, g_aDiskFullScreenColors[ iDrive1Status ] );
		//		TextOut(dc,x+ 10,y+2,TEXT("*"),1);
		//		SetTextColor(dc, g_aDiskFullScreenColors[ iDrive2Status ] );
		//		TextOut(dc,x+ 20,y+2,TEXT("*"),1);

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
			DrawBitmapRect(dc,x+12,y+8,&rect,g_hDiskWindowedLED[iDrive1Status]);
			DrawBitmapRect(dc,x+30,y+8,&rect,g_hDiskWindowedLED[iDrive2Status]);

			if (!IS_APPLE2)
			{
				RECT rect = {0,0,31,8};
			switch (g_Apple2Type)
			{
			case A2TYPE_APPLE2:			DrawBitmapRect(dc,x+7,y+9,&rect,capsbitmap[bCaps != 0]); break; 
			case A2TYPE_APPLE2PLUS:		DrawBitmapRect(dc,x+7,y+19,&rect,capsbitmap[bCaps != 0]); break; 
			case A2TYPE_APPLE2E:		DrawBitmapRect(dc,x+7,y+19,&rect,capsbitmap[bCaps != 0]); break; 
			case A2TYPE_APPLE2EEHANCED:	DrawBitmapRect(dc,x+7,y+19,&rect,capsbitmap[bCaps != 0]); break; 
			case A2TYPE_PRAVETS82:		DrawBitmapRect(dc,x+15,y+19,&rect,latbitmap[bCaps != 0]); break; 
			case A2TYPE_PRAVETS8M:		DrawBitmapRect(dc,x+15,y+19,&rect,latbitmap[bCaps != 0]); break; 
			case A2TYPE_PRAVETS8A:		DrawBitmapRect(dc,x+2,y+19,&rect,latbitmap[bCaps != 0]); break; 
			}
			if (g_Apple2Type == A2TYPE_PRAVETS8A) //Toggles Pravets 8A/C Caps lock LED
			{
				RECT rect = {0,0,22,8};
				DrawBitmapRect(dc,x+23,y+19,&rect,capsbitmapP8[P8CAPS_ON != 0]);
			}

		
			/*
			if (g_Apple2Type == A2TYPE_PRAVETS8A)
					DrawBitmapRect(dc,x+7,y+19,&rect,cyrbitmap[bCaps != 0]);
				else
					DrawBitmapRect(dc,x+7,y+19,&rect,capsbitmap[bCaps != 0]);
*/
			}
		}

		if (drawflags & DRAW_TITLE)
		{
			TCHAR title[80];
			switch (g_Apple2Type)
			{
			default:
			case A2TYPE_APPLE2:			_tcscpy(title, TITLE_APPLE_2); break; 
			case A2TYPE_APPLE2PLUS:		_tcscpy(title, TITLE_APPLE_2_PLUS); break; 
			case A2TYPE_APPLE2E:		_tcscpy(title, TITLE_APPLE_2E); break; 
			case A2TYPE_APPLE2EEHANCED:	_tcscpy(title, TITLE_APPLE_2E_ENHANCED); break; 
			case A2TYPE_PRAVETS82:		_tcscpy(title, TITLE_PRAVETS_82); break; 
			case A2TYPE_PRAVETS8M:		_tcscpy(title, TITLE_PRAVETS_8M); break; 
			case A2TYPE_PRAVETS8A:		_tcscpy(title, TITLE_PRAVETS_8A); break; 
			}

			// TODO: g_bDisplayVideoModeInTitle
			if( 1 )
			{
				_tcscat( title, " - " );

				if( g_uHalfScanLines )
				{
					_tcscat( title," 50% " );
				}

				extern char *g_apVideoModeDesc[ NUM_VIDEO_MODES ];
				_tcscat( title, g_apVideoModeDesc[ g_eVideoType ] );
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
static void EraseButton (int number) {
  RECT rect;
  rect.left   = buttonx;
  rect.right  = rect.left+BUTTONCX;
  rect.top    = buttony+number*BUTTONCY;
  rect.bottom = rect.top+BUTTONCY;

	// TODO: DD Full-Screen Palette
	//	if( !g_bIsFullScreen )

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
    case WM_ACTIVATE:		// Sent when window is activated/deactivated. wParam indicates WA_ACTIVE, WA_INACTIVE, etc
							// Eg. Deactivate when Config dialog is active, AppleWin app loses focus, etc
      JoyReset();
      SetUsingCursor(0);
	  RevealCursor();
      break;

    case WM_ACTIVATEAPP:	// Sent when different app's window is activated/deactivated.
							// Eg. Deactivate when AppleWin app loses focus
      g_bAppActive = (wparam ? TRUE : FALSE);
      break;

    case WM_CLOSE:
      if (g_bIsFullScreen)
        SetNormalMode();
      if (!IsIconic(window))
        GetWindowRect(window,&framerect);
      RegSaveValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_X_POS), 1, framerect.left);
      RegSaveValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_Y_POS), 1, framerect.top);
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
				if( !g_bDebuggerEatKey )
				{
					KeybQueueKeypress((int)wparam,ASCII);
				} else {
					g_bDebuggerEatKey = false;
				}
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
//MessageBox( NULL, filename, "DDE Exec", MB_OK );
      ImageError_e Error = DiskInsert(DRIVE_1, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
      if (Error == eIMAGE_ERROR_NONE)
	  {
        if (!g_bIsFullScreen)
          DrawButton((HDC)0,BTN_DRIVE1);

		PostMessage(window, WM_USER_BOOT, 0, 0);
      }
      else
      {
        DiskNotifyInvalidImage(DRIVE_1, filename, Error);
      }
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
	  const int iDrive = PtInRect(&rect,point) ? DRIVE_2 : DRIVE_1;
      ImageError_e Error = DiskInsert(iDrive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
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
        DiskNotifyInvalidImage(iDrive, filename, Error);
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
#if _DEBUG
//			MessageBox( NULL, "Double 580x384 size!", "PrintScreen", MB_OK );
#endif
			Video_TakeScreenShot( SCREENSHOT_560x384 );
		}
		else
		if (wparam == VK_SNAPSHOT_280)
		{
			if( lparam & MOD_SHIFT)
			{
#if _DEBUG
//				MessageBox( NULL, "Normal 280x192 size!", "PrintScreen", MB_OK );
#endif
			}
			Video_TakeScreenShot( SCREENSHOT_280x192 );
		}
		break;

	case WM_KEYDOWN:
		KeybUpdateCtrlShiftStatus();

		if ((wparam >= VK_F1) && (wparam <= VK_F8) && (buttondown == -1))
		{
			SetUsingCursor(0);
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
			//bool bCtrlDown  = (GetKeyState(VK_CONTROL) < 0) ? true : false;
			//bool bShiftDown = (GetKeyState(VK_SHIFT  ) < 0) ? true : false;

// F9 Next Video Mode
// ^F9 Next Char Set
// #F9 Prev Video Mode
// ^#F9 Toggle 50% Scan Lines
// @F9 -Can't use Alt-F9 as Alt is Open-Apple = Joystick Button #1

			if ( g_bCtrlKey && !g_bShiftKey ) //CTRL+F9
			{
				g_nCharsetType++; // Cycle through available charsets (Ctrl + F9)
				if (g_nCharsetType >= 3)
				{				
					g_nCharsetType = 0;
				}
			}
			else	// Cycle through available video modes
			if ( g_bCtrlKey && g_bShiftKey ) // ALT+F9
			{
				g_uHalfScanLines = !g_uHalfScanLines;
			}
			else
			if ( !g_bShiftKey )	// Drop Down Combo Box is in correct order
			{
				g_eVideoType++;
				if (g_eVideoType >= NUM_VIDEO_MODES)
					g_eVideoType = 0;
			}
			else	// Forwards
			{
				if (g_eVideoType <= 0)
					g_eVideoType = NUM_VIDEO_MODES;
				g_eVideoType--;
			}

			// TODO: Clean up code:FrameRefreshStatus(DRAW_TITLE) DrawStatusArea((HDC)0,DRAW_TITLE)
			DrawStatusArea( (HDC)0, DRAW_TITLE );

			VideoReinitialize();
			if ((g_nAppMode != MODE_LOGO) || ((g_nAppMode == MODE_DEBUG) && (g_bDebuggerViewingAppleOutput)))
			{
				VideoRedrawScreen();
				g_bDebuggerViewingAppleOutput = true;
			}

			Config_Save_Video();
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
					RevealCursor();
					break;
				case MODE_PAUSED:
					g_nAppMode = MODE_RUNNING;
					SoundCore_SetFade(FADE_IN);
					// Don't call FrameShowCursor(FALSE) else ClipCursor() won't be called
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
		{		
			DebuggerProcessKey(wparam);
		}

		if (wparam == VK_F10)
		{
			if ((g_Apple2Type == A2TYPE_PRAVETS8A) && (GetKeyState(VK_CONTROL) >= 0))
			{
				KeybToggleP8ACapsLock ();//Toggles P8 Capslock
			}
			else 
			{
				SetUsingCursor(0);
				return 0;	// TC: Why return early?
			}
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
        else if (g_bUsingCursor && !sg_Mouse.IsActive())
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
				RevealCursor();
			}
			else if (g_nAppMode == MODE_RUNNING)
			{
				if (!sg_Mouse.IsEnabled())
				{
					sg_Mouse.SetEnabled(true);

					POINT Point;
					GetCursorPos(&Point);
					ScreenToClient(g_hFrameWindow, &Point);
					const int iOutOfBoundsX=0, iOutOfBoundsY=0;
					UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY, Point.x, Point.y);

					// Don't call SetButton() when 1st enabled (else get the confusing action of both enabling & an Apple mouse click)
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
          if (g_bIsFullScreen)
            EraseButton(buttonactive);
          else
            DrawButton((HDC)0,buttonactive);
          ProcessButtonClick(buttonactive);
        }
        buttonactive = -1;
      }
      else if (g_bUsingCursor && !sg_Mouse.IsActive())
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
      else if (g_bIsFullScreen && (newover != buttonover) && (buttondown == -1)) {
        if (buttonover != -1)
          EraseButton(buttonover);
        buttonover = newover;
        if (buttonover != -1)
          DrawButton((HDC)0,buttonover);
      }
      else if (g_bUsingCursor && !sg_Mouse.IsActive())
	  {
        DrawCrosshairs(x,y);
	    JoySetPosition(x-viewportx-2, VIEWPORTCX-4, y-viewporty-2, VIEWPORTCY-4);
      }
	  else if (sg_Mouse.IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING))
	  {
			if (g_bLastCursorInAppleViewport)
				break;

			// Outside Apple viewport

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
			// NB. Need to check /g_bAppActive/ since WM_TIMER events still occur after AppleWin app has lost focus
			if (g_bAppActive && sg_Mouse.IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING))
			{
				if (!g_bLastCursorInAppleViewport)
					break;

				// Inside Apple viewport

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

    case WM_NOTIFY:	// Tooltips for Drive buttons
      if(((LPNMTTDISPINFO)lparam)->hdr.hwndFrom == tooltipwindow &&
         ((LPNMTTDISPINFO)lparam)->hdr.code == TTN_GETDISPINFO)
        ((LPNMTTDISPINFO)lparam)->lpszText =
          (LPTSTR)DiskGetFullDiskFilename(((LPNMTTDISPINFO)lparam)->hdr.idFrom);
      break;

    case WM_PAINT:
      if (GetUpdateRect(window,NULL,0)) {
        g_bPaintingWindow = 1;
        DrawFrameWindow();
        g_bPaintingWindow = 0;
      }
      break;

    case WM_PALETTECHANGED:
		// To avoid creating an infinite loop, a window that receives this
		// message must not realize its palette, unless it determines that
		// wParam does not contain its own window handle.
		if ((HWND)wparam == window)
	  {
#if DEBUG_DD_PALETTE
		if( g_bIsFullScreen )
			OutputDebugString( "WM_PALETTECHANGED: Full Screen\n" );
		else
			OutputDebugString( "WM_PALETTECHANGED: Windowed\n" );
#endif
		break;
	  } 
	  // else fall through

    case WM_QUERYNEWPALETTE:
#if DEBUG_DD_PALETTE
		if( g_bIsFullScreen )
			OutputDebugString( "WM_QUERYNEWPALETTE: Full Screen\n" );
		else
			OutputDebugString( "WM_QUERYNEWPALETTE: Windowed\n" );
#endif

		// TODO: DD Full-Screen Palette
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

					FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
					DrawButton((HDC)0,iButton);
				}			
			}
		}
		if (g_bUsingCursor)
		{
			if (sg_Mouse.IsActive())
				sg_Mouse.SetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);
			else
				JoySetButton(BUTTON1, (message == WM_RBUTTONDOWN) ? BUTTON_DOWN : BUTTON_UP);
		}
		RelayEvent(message,wparam,lparam);
		break;

    case WM_SYSCOLORCHANGE:
#if DEBUG_DD_PALETTE
		if( g_bIsFullScreen )
			OutputDebugString( "WM_SYSCOLORCHANGE: Full Screen\n" );
		else
			OutputDebugString( "WM_SYSCOLORCHANGE: Windowed\n" );
#endif

		// TODO: DD Full-Screen Palette
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
				sg_SSC.CommTcpSerialClose();
				break;

			default:
				sg_SSC.CommTcpSerialCleanup();
				break;
			}
		}
		else
		{
			WORD wSelectEvent = WSAGETSELECTEVENT(lparam);
			switch(wSelectEvent)
			{
				case FD_ACCEPT:
					sg_SSC.CommTcpSerialAccept();
					break;

				case FD_CLOSE:
					sg_SSC.CommTcpSerialClose();
					break;

				case FD_READ:
					sg_SSC.CommTcpSerialReceive();
					break;

				case FD_WRITE:
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

  }	// switch(message)
 
  return DefWindowProc(window,message,wparam,lparam);
}



//===========================================================================
void ProcessButtonClick (int button)
{
	SoundCore_SetFade(FADE_OUT);

#if DEBUG_DD_PALETTE
	char _text[ 80 ];
	sprintf( _text, "Button: F%d  Full Screen: %d\n", button+1, g_bIsFullScreen );
	OutputDebugString( _text );
#endif

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
		KeybUpdateCtrlShiftStatus();
		if( g_bCtrlKey )
		{
			CtrlReset();
			return;
		}

		if (g_nAppMode == MODE_LOGO)
		{
			DiskBoot();
			g_nAppMode = MODE_RUNNING;
		}
		else
		if (g_nAppMode == MODE_RUNNING)
		{
			ResetMachineState();
			g_nAppMode = MODE_RUNNING;
		}
		if ((g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING))
		{
			// If any breakpoints active, 
			if (g_nBreakpoints)
			{
				// switch to MODE_STEPPING
				CmdGo( 0 );
			}
			else
			{
				DebugEnd();
				g_nAppMode = MODE_RUNNING;
			}
		}
      DrawStatusArea((HDC)0,DRAW_TITLE);
      VideoRedrawScreen();
      g_bResetTiming = true;
      break;

    case BTN_DRIVE1:
    case BTN_DRIVE2:
      DiskSelect(button-BTN_DRIVE1);
      if (!g_bIsFullScreen)
        DrawButton((HDC)0,button);
      break;

    case BTN_DRIVESWAP:
      DiskDriveSwap();
      break;

    case BTN_FULLSCR:
      if (g_bIsFullScreen)
        SetNormalMode();
      else
        SetFullScreenMode();
      break;

    case BTN_DEBUG:
		if (g_nAppMode == MODE_LOGO)
		{
			ResetMachineState();
		}

		// bug/feature: allow F7 to enter debugger even though emulator isn't "running"
		//else

		if (g_nAppMode == MODE_STEPPING)
		{
			DebuggerInputConsoleChar( DEBUG_EXIT_KEY );
		}
		else
		if (g_nAppMode == MODE_DEBUG)
		{
			g_bDebugDelayBreakCheck = true;
			ProcessButtonClick(BTN_RUN);

			// TODO: DD Full-Screen Palette
			// exiting debugger using wrong palette, but this makes problem worse...
			//InvalidateRect(g_hFrameWindow,NULL,1);
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

// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/winui/windowsuserinterface/resources/menus/usingmenus.asp
// http://www.codeproject.com/menu/MenusForBeginners.asp?df=100&forumid=67645&exp=0&select=903061

void ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive)
{		
	//This is the default installation path of CiderPress. It shall not be left blank, otherwise  an explorer window will be open.
	TCHAR PathToCiderPress[MAX_PATH] = "C:\\Program Files\\faddenSoft\\CiderPress\\CiderPress.exe";
	RegLoadString(TEXT("Configuration"), REGVALUE_CIDERPRESSLOC, 1, PathToCiderPress,MAX_PATH);
	//TODO: A directory is open if an empty path to CiderPress is set. This has to be fixed.

	string filename1= "\"";
	filename1.append (DiskPathFilename[iDrive]);
	filename1.append ("\"");
	string sFileNameEmpty = "\"";
	sFileNameEmpty.append ("\"");

	//  Load the menu template containing the shortcut menu from the
	//  application's resources.
	HMENU hmenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_DISK_POPUP));	// menu template
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
		if (DiskGetProtect( iDrive ))
			iMenuItem = ID_DISKMENU_WRITEPROTECTION_ON;

		CheckMenuItem(hmenu, iMenuItem, MF_CHECKED);
	}

	if (Disk_IsDriveEmpty(iDrive))
		EnableMenuItem(hmenu, ID_DISKMENU_EJECT, MF_GRAYED);

	if (Disk_ImageIsWriteProtected(iDrive))
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
		DiskEject( iDrive );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_ON)
		DiskSetProtect( iDrive, true );
	else
	if (iCommand == ID_DISKMENU_WRITEPROTECTION_OFF)
		DiskSetProtect( iDrive, false );
	else
	if (iCommand == ID_DISKMENU_SENDTO_CIDERPRESS)
	{
		//if(!filename1.compare("\"\"") == false) //Do not use this, for some reason it does not work!!!
		if(!filename1.compare(sFileNameEmpty) )
		{
			int MB_Result = MessageBox( NULL, "No disk image loaded. Do you want to run CiderPress anyway?" ,"No disk image.", MB_ICONINFORMATION|MB_YESNO );
			if (MB_Result == IDYES)
			{
				if (FileExists (PathToCiderPress ))
				{
					HINSTANCE nResult  = ShellExecute(NULL, "open", PathToCiderPress, "" , NULL, SW_SHOWNORMAL);
				}
				else
				{
					MessageBox( NULL,
						"CiderPress not found!\n"
						"Please install CiderPress in case it is not \n"
						"or set the path to it from Configuration/Disk otherwise."
							, "CiderPress not found" ,MB_ICONINFORMATION|MB_OK);
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
				MessageBox( NULL,
					"CiderPress not found!\n"
					"Please install CiderPress in case it is not \n"
					"or set the path to it from Configuration/Disk otherwise."
					, "CiderPress not found" ,MB_ICONINFORMATION|MB_OK);
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
  msg.hwnd    = g_hFrameWindow;
  msg.message = message;
  msg.wParam  = wparam;
  msg.lParam  = lparam;
  SendMessage(tooltipwindow,TTM_RELAYEVENT,0,(LPARAM)&msg);
}

//===========================================================================
void ResetMachineState ()
{
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
#ifdef SUPPORT_CPM
  g_ActiveCPU = CPU_6502;
#endif

  SoundCore_SetFade(FADE_NONE);
}


//===========================================================================
void CtrlReset()
{
	// Ctrl+Reset - TODO: This is a terrible place for this code!
	if (!IS_APPLE2)
		MemResetPaging();

	DiskReset();
	KeybReset();
	if (!IS_APPLE2)
		VideoResetState();	// Switch Alternate char set off
	sg_SSC.CommReset();
	MB_Reset();

	CpuReset();
	g_bFreshReset = true;
}


//===========================================================================
void SetFullScreenMode ()
{
	g_bIsFullScreen = true;
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
	if (DirectDrawCreate(NULL,&g_pDD,NULL) != DD_OK ||
		g_pDD->SetCooperativeLevel(g_hFrameWindow,DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN) != DD_OK ||
		g_pDD->SetDisplayMode(640,480,8) != DD_OK ||
		g_pDD->CreateSurface(&ddsd,&g_pDDPrimarySurface,NULL) != DD_OK)
	{
		g_pDDPrimarySurface = NULL;
		SetNormalMode();
		return;
	}

	// TODO: DD Full-Screen Palette
	//	if( !g_bIsFullScreen )

	InvalidateRect(g_hFrameWindow,NULL,1);
}

//===========================================================================
void SetNormalMode ()
{
	g_bIsFullScreen = false;
	buttonover = -1;
	buttonx    = BUTTONX;
	buttony    = BUTTONY;
	viewportx  = VIEWPORTX;
	viewporty  = VIEWPORTY;
	g_pDD->RestoreDisplayMode();
	g_pDD->SetCooperativeLevel(NULL,DDSCL_NORMAL);
	SetWindowLong(g_hFrameWindow,GWL_STYLE,
                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX |
                WS_VISIBLE);
	SetWindowPos(g_hFrameWindow,0,framerect.left,
                             framerect.top,
                             framerect.right - framerect.left,
                             framerect.bottom - framerect.top,
                             SWP_NOZORDER | SWP_FRAMECHANGED);

	// DD Full-Screen Palette: BUGFIX: Re-attach new palette on next new surface
	// Delete Palette
	if (g_pDDPal)
	{
		g_pDDPal->Release();
		g_pDDPal = (LPDIRECTDRAWPALETTE)0;
	}

	if (g_pDDPrimarySurface)
	{
		g_pDDPrimarySurface->Release();
		g_pDDPrimarySurface = (LPDIRECTDRAWSURFACE)0;
	}

	g_pDD->Release();
	g_pDD = (LPDIRECTDRAW)0;
}

//===========================================================================
void SetUsingCursor (BOOL bNewValue)
{
	if (bNewValue == g_bUsingCursor)
		return;

	g_bUsingCursor = bNewValue;
	if (g_bUsingCursor)
	{
		SetCapture(g_hFrameWindow);
		RECT rect =	{	viewportx+2,				// left
						viewporty+2,				// top
						viewportx+VIEWPORTCX-1,		// right
						viewporty+VIEWPORTCY-1};	// bottom
		ClientToScreen(g_hFrameWindow,(LPPOINT)&rect.left);
		ClientToScreen(g_hFrameWindow,(LPPOINT)&rect.right);
		ClipCursor(&rect);
		FrameShowCursor(FALSE);
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(g_hFrameWindow,&pt);
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

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void FrameCreateWindow ()
{
	const int nWidth  = VIEWPORTCX + VIEWPORTX*2
								   + BUTTONCX
								   + GetSystemMetrics(SM_CXBORDER)*2
								   + MAGICX;
	const int nHeight = VIEWPORTCY + VIEWPORTY*2
								   + GetSystemMetrics(SM_CYBORDER)
								   + GetSystemMetrics(SM_CYCAPTION)
								   + MAGICY;

	//

	int nXPos = -1;
	{
		int nXScreen = GetSystemMetrics(SM_CXSCREEN) - nWidth;

		if (RegLoadValue(TEXT("Preferences"), TEXT("Window X-Position"), 1, (DWORD*)&nXPos))
		{
			if (nXPos > nXScreen)
				nXPos = -1;	// Not fully visible, so default to centre position
		}

		if (nXPos == -1)
			nXPos = nXScreen / 2;
	}

	//

	int nYPos = -1;
	{
		int nYScreen = GetSystemMetrics(SM_CYSCREEN) - nHeight;

		if (RegLoadValue(TEXT("Preferences"), TEXT("Window Y-Position"), 1, (DWORD*)&nYPos))
		{
			if (nYPos > nYScreen)
				nYPos = -1;	// Not fully visible, so default to centre position
		}

		if (nYPos == -1)
			nYPos = nYScreen / 2;
	}

	//

	switch (g_Apple2Type)
	{
	case A2TYPE_APPLE2:			g_pAppTitle = TITLE_APPLE_2; break; 
	case A2TYPE_APPLE2PLUS:		g_pAppTitle = TITLE_APPLE_2_PLUS; break; 
	case A2TYPE_APPLE2E:		g_pAppTitle = TITLE_APPLE_2E; break; 
	case A2TYPE_APPLE2EEHANCED:	g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break; 
	case A2TYPE_PRAVETS82:	    g_pAppTitle = TITLE_PRAVETS_82; break; 
	case A2TYPE_PRAVETS8M:	    g_pAppTitle = TITLE_PRAVETS_8M; break; 
	case A2TYPE_PRAVETS8A:	    g_pAppTitle = TITLE_PRAVETS_8A; break; 
	}


	g_hFrameWindow = CreateWindow(
		TEXT("APPLE2FRAME"),
		g_pAppTitle,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_VISIBLE,
		nXPos, nYPos, nWidth, nHeight,
		HWND_DESKTOP,
		(HMENU)0,
		g_hInstance, NULL );


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
HDC FrameGetVideoDC (LPBYTE *pAddr_, LONG *pPitch_)
{
	// ASSERT( pAddr_ );
	// ASSERT( pPitch_ );
	if (g_bIsFullScreen && g_bAppActive && !g_bPaintingWindow)
	{
		RECT rect = {	FSVIEWPORTX,
						FSVIEWPORTY,
						FSVIEWPORTX+VIEWPORTCX,
						FSVIEWPORTY+VIEWPORTCY};
		DDSURFACEDESC surfacedesc;
		surfacedesc.dwSize = sizeof(surfacedesc);
		// TC: Use DDLOCK_WAIT - see Bug #13425
		if (g_pDDPrimarySurface->Lock(&rect,&surfacedesc,DDLOCK_WAIT,NULL) == DDERR_SURFACELOST)
		{
			g_pDDPrimarySurface->Restore();
			g_pDDPrimarySurface->Lock(&rect,&surfacedesc,DDLOCK_WAIT,NULL);

			// DD Full Screen Palette
//			if (g_pDDPal)
//			{
//				g_pDDPrimarySurface->SetPalette(g_pDDPal); // this sets the palette for the primary surface
//			}
		}
		*pAddr_  = (LPBYTE)surfacedesc.lpSurface+(VIEWPORTCY-1)*surfacedesc.lPitch;
		*pPitch_ = -surfacedesc.lPitch;

		return (HDC)0;
	}
	else
	{
		*pAddr_ = g_pFramebufferbits;
		*pPitch_ = FRAMEBUFFER_W;
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
void FrameReleaseVideoDC ()
{
	if (g_bIsFullScreen && g_bAppActive && !g_bPaintingWindow)
	{
		// THIS IS CORRECT ACCORDING TO THE DIRECTDRAW DOCS
		RECT rect = {
			FSVIEWPORTX,
			FSVIEWPORTY,
			FSVIEWPORTX+VIEWPORTCX,
			FSVIEWPORTY+VIEWPORTCY
		};
		g_pDDPrimarySurface->Unlock(&rect);

		// BUT THIS SEEMS TO BE WORKING
		g_pDDPrimarySurface->Unlock(NULL);
	}
}

//===========================================================================
// TODO: FIXME: Util_TestFileExists()
static bool FileExists(string strFilename) 
{ 
	struct stat stFileInfo; 
	int intStat = stat(strFilename.c_str(),&stFileInfo); 
	return (intStat == 0) ? true : false;
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

// Called when:
// . UpdateMouseInAppleViewport() is called and mouse leaving/entering Apple screen area
// . NB. Not called when leaving & mouse clipped to Apple screen area
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
		x -= (viewportx+2-MAGICX); if (x < 0) x = 0;
		y -= (viewporty+2-MAGICY); if (y < 0) y = 0;

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
		if (g_uMouseRestrictToWindow)
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

			if (g_uMouseRestrictToWindow)
				SetUsingCursor(TRUE);
		}
		else
		{
			FrameSetCursorPosByMousePos();	// Set cursor to Apple position each time
		}

		DrawCrosshairsMouse();
	}
}
