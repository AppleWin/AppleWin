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
#include <sstream>
#include <ctime>
#if _MSVC_LANG >= 201703L // Compiler option: /std:c++17
	#define _HAS_CXX17 1
	#include <filesystem>
#endif
#include "Windows/Win32Frame.h"
#include "Windows/AppleWin.h"
#include "CmdLine.h"
#include "Interface.h"
#include "Keyboard.h"
#include "Log.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "Windows/DirectInput.h"
#include "Windows/DXSoundBuffer.h"
#include "NTSC.h"
#include "ParallelPrinter.h"
#include "Pravets.h"
#include "Registry.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "Uthernet1.h"
#include "Uthernet2.h"
#include "Speaker.h"
#include "Utilities.h"
#include "CardManager.h"
#include "../resource/resource.h"
#include "Configuration/PropertySheet.h"
#include "Debugger/Debug.h"
#include "../ProDOS_FileSystem.h"
#include "../resource/resource.h"

//#define ENABLE_MENU 0
#define DEBUG_KEY_MESSAGES 0

static bool FileExists(std::string strFilename);

// Must keep in sync with Disk_Status_e g_aDiskFullScreenColors
static const uint32_t g_aDiskFullScreenColorsLED[ NUM_DISK_STATUS ] =
{
	RGB(  0,  0,  0), // DISK_STATUS_OFF   BLACK
	RGB(  0,255,  0), // DISK_STATUS_READ  GREEN
	RGB(255,  0,  0), // DISK_STATUS_WRITE RED
	RGB(255,128,  0), // DISK_STATUS_PROT  ORANGE
	RGB(  0,  0,255), // DISK_STATUS_EMPTY -blue-
	RGB(  0,128,128)  // DISK_STATUS_SPIN  -cyan-
};

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

UINT Win32Frame::Get3DBorderWidth(void)
{
	return IsFullScreen() ? 0 : VIEWPORTX;
}

UINT Win32Frame::Get3DBorderHeight(void)
{
	return IsFullScreen() ? 0 : VIEWPORTY;
}

//===========================================================================

void Win32Frame::FrameShowCursor(BOOL bShow)
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
void Win32Frame::RevealCursor()
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
void Win32Frame::FullScreenRevealCursor(void)
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

#define LOADBUTTONBITMAP(bitmapname)  LoadImage(g_hInstance,bitmapname,   \
                                                IMAGE_BITMAP,0,0,      \
                                                LR_CREATEDIBSECTION |  \
                                                LR_LOADMAP3DCOLORS |   \
                                                LR_LOADTRANSPARENT);

void Win32Frame::CreateGdiObjects(void)
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
	g_hDiskWindowedLED[ DISK_STATUS_EMPTY] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKOFF_BITMAP"));
	g_hDiskWindowedLED[ DISK_STATUS_SPIN ] = (HBITMAP)LOADBUTTONBITMAP(TEXT("DISKREAD_BITMAP"));

	btnfacebrush    = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
	btnfacepen      = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNFACE));
	btnhighlightpen = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));
	btnshadowpen    = CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
	smallfont = CreateFont(smallfontHeight,6,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,
							OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY,VARIABLE_PITCH | FF_SWISS,
							TEXT("Small Fonts"));
}

//===========================================================================
void Win32Frame::DeleteGdiObjects(void)
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
void Win32Frame::Draw3dRect(HDC dc, int x1, int y1, int x2, int y2, BOOL out)
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
void Win32Frame::DrawBitmapRect (HDC dc, int x, int y, LPRECT rect, HBITMAP bitmap) {
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
void Win32Frame::DrawButton (HDC passdc, int number) {
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

	LPCTSTR pszBaseName = (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		? dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).GetBaseName(number-BTN_DRIVE1).c_str()
		: "";

    ExtTextOut(dc,x+offset+22,rect.top,ETO_CLIPPED,&rect,
               pszBaseName,
               MIN(8,_tcslen(pszBaseName)),
               NULL);
  }
  if (!passdc)
    ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================

// NB. x=y=0 means erase only
void Win32Frame::DrawCrosshairs (int x, int y) {
  static int lastx = 0;
  static int lasty = 0;
  FrameReleaseDC();
  HDC dc = GetDC(g_hFrameWindow);
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
  ReleaseDC(g_hFrameWindow,dc);
}

//===========================================================================
void Win32Frame::DrawFrameWindow (bool bPaintingWindow/*=false*/)
{
	FrameReleaseDC();
	PAINTSTRUCT ps;
	HDC         dc = bPaintingWindow
		? BeginPaint(g_hFrameWindow,&ps)
		: GetDC(g_hFrameWindow);

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

		if (g_nViewportScale == 2 || GetVideo().HasVidHD())
		{
			const int x = buttonx + 1;
			const int y = buttony + BUTTONS * BUTTONCY + 36;	// 36 = height of StatusArea
			RECT rect = { x, y, x + BUTTONCX, y + BUTTONS * BUTTONCY + 22 };

			if (GetVideo().HasVidHD())
			{
				if (g_nViewportScale == 1)
					rect.bottom += 14;
				else
					rect.bottom += 32;
			}

			int res = FillRect(dc, &rect, btnfacebrush);
		}
	}

	// DRAW THE STATUS AREA
	DrawStatusArea(dc,DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);

	// DRAW THE CONTENTS OF THE EMULATED SCREEN
	if (g_nAppMode == MODE_LOGO)
		DisplayLogo();
	else if (g_nAppMode == MODE_DEBUG)
		DebugDisplay();
	else
		VideoRedrawScreen();

	if (bPaintingWindow)
		EndPaint(g_hFrameWindow,&ps);
	else
		ReleaseDC(g_hFrameWindow,dc);

}

//===========================================================================

bool Win32Frame::IsFullScreen(void)
{
	return g_bIsFullScreen;
}

bool Win32Frame::GetFullScreenShowSubunitStatus(void)
{
	return g_bFullScreen_ShowSubunitStatus;
}

void Win32Frame::SetFullScreenShowSubunitStatus(bool bShow)
{
	g_bFullScreen_ShowSubunitStatus = bShow;
}

bool Win32Frame::GetWindowedModeShowDiskiiStatus(void)
{
	return m_showDiskiiStatus;
}

void Win32Frame::SetWindowedModeShowDiskiiStatus(bool bShow)
{
	m_showDiskiiStatus = bShow;
	m_redrawDiskiiStatus = true;
	SetSlotUIOffsets();
}

void Win32Frame::SetSlotUIOffsets(void)
{
	if (m_showDiskiiStatus)
	{
		yOffsetSlot5Label = D2FullUI::yOffsetSlot5Label;
		yOffsetSlot5LEDNumbers = D2FullUI::yOffsetSlot5LEDNumbers;
		yOffsetSlot5LEDs = D2FullUI::yOffsetSlot5LEDs;
	}
	else
	{
		yOffsetSlot5Label = D2CompactUI::yOffsetSlot5Label;
		yOffsetSlot5LEDNumbers = D2CompactUI::yOffsetSlot5LEDNumbers;
		yOffsetSlot5LEDs = D2CompactUI::yOffsetSlot5LEDs;
	}
}

void Win32Frame::FrameDrawDiskLEDS()
{
	FrameDrawDiskLEDS((HDC)0);
}

//===========================================================================
void Win32Frame::FrameDrawDiskLEDS( HDC passdc )
{
	g_eStatusDrive1 = DISK_STATUS_OFF;
	g_eStatusDrive2 = DISK_STATUS_OFF;

	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).GetLightStatus(&g_eStatusDrive1, &g_eStatusDrive2);

	// Draw Track/Sector
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(g_hFrameWindow));
	const int x = buttonx;
	const int y = buttony + BUTTONS * BUTTONCY + 1;

	if (g_bIsFullScreen)
	{
		if (!g_bFullScreen_ShowSubunitStatus)
			return;

		SelectObject(dc,smallfont);
		SetBkMode(dc,OPAQUE);
		SetBkColor(dc,RGB(0,0,0));
		SetTextAlign(dc,TA_LEFT | TA_TOP);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive1]);
		TextOut(dc,x+ 3,y+2,TEXT("1"),1);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive2]);
		TextOut(dc,x+13,y+2,TEXT("2"),1);
	}
	else
	{
		RECT rDiskLed = { 0,0,8,8 };
		DrawBitmapRect(dc, x + 12, y + yOffsetSlot6LEDs, &rDiskLed, g_hDiskWindowedLED[g_eStatusDrive1]);
		DrawBitmapRect(dc, x + 31, y + yOffsetSlot6LEDs, &rDiskLed, g_hDiskWindowedLED[g_eStatusDrive2]);

		if (g_nViewportScale > 1 && GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
		{
			Disk_Status_e eDrive1StatusSlot5 = DISK_STATUS_OFF;
			Disk_Status_e eDrive2StatusSlot5 = DISK_STATUS_OFF;
			dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT5)).GetLightStatus(&eDrive1StatusSlot5, &eDrive2StatusSlot5);

			DrawBitmapRect(dc, x + 12, y + yOffsetSlot5LEDs, &rDiskLed, g_hDiskWindowedLED[eDrive1StatusSlot5]);
			DrawBitmapRect(dc, x + 31, y + yOffsetSlot5LEDs, &rDiskLed, g_hDiskWindowedLED[eDrive2StatusSlot5]);
		}
	}
}

void Win32Frame::FrameDrawDiskStatus()
{
	FrameDrawDiskStatus((HDC)0);
}

void Win32Frame::GetTrackSector(UINT slot, int& drive1Track, int& drive2Track, int& activeFloppy)
{
	//        DOS3.3   ProDOS
	// Slot   $B7E9    $BE3C(DEFSLT=Default Slot)      ; ref: Beneath Apple ProDOS 8-3. Not ProDOS v1.9 (16-JUL-90)
	// Drive  $B7EA    $BE3D(DEFDRV=Default Drive)     ; ref: Beneath Apple ProDOS 8-3
	// Track  $B7EC    LC1 $D356
	// Sector $B7ED    LC1 $D357
	// Unit   -        LC1 $D359					   ; ref: Beneath Apple ProDOS 1.20 and 1.30 Supplement, p83
	// RWTS            LC1 $D300

	drive1Track = drive2Track = activeFloppy = 0;
	if (GetCardMgr().QuerySlot(slot) != CT_Disk2)
		return;

	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));
	activeFloppy = disk2Card.GetCurrentDrive();
	drive1Track = disk2Card.GetTrack(DRIVE_1);
	drive2Track = disk2Card.GetTrack(DRIVE_2);

	// Probe known OS's for default Slot/Track/Sector
	const bool isProDOS = mem[0xBF00] == 0x4C;
	bool isSectorValid = false;
	int drive1Sector = -1, drive2Sector = -1;

	// Try DOS3.3 Sector
	if (!isProDOS)
	{
		const int nDOS33slot = mem[0xB7E9] / 16;
		const int nDOS33track = mem[0xB7EC];
		const int nDOS33sector = mem[0xB7ED];

		if ((nDOS33slot == slot)
			&& (nDOS33track >= 0 && nDOS33track < 40)
			&& (nDOS33sector >= 0 && nDOS33sector < 16))
		{
#if _DEBUG && 0
			if (nDOS33track != nDisk1Track)
			{
				LogOutput("\n\n\nWARNING: DOS33Track: %d (%02X) != nDisk1Track: %d (%02X)\n\n\n", nDOS33track, nDOS33track, nDisk1Track, nDisk1Track);
			}
#endif // _DEBUG

			/**/ if (activeFloppy == 0) drive1Sector = nDOS33sector;
			else if (activeFloppy == 1) drive2Sector = nDOS33sector;

			isSectorValid = true;
		}
	}
	else // isProDOS
	{
		// we can't just read from mem[ 0xD357 ] since it might be bank-switched from ROM
		// and we need the Language Card RAM
		const int nProDOStrack = *MemGetMainPtrWithLC(0xC356); // LC1 $D356
		const int nProDOSsector = *MemGetMainPtrWithLC(0xC357); // LC1 $D357
		const int nProDOSslot = *MemGetMainPtrWithLC(0xC359) / 16; // LC1 $D359

		if ((nProDOSslot == slot)
			&& (nProDOStrack >= 0 && nProDOStrack < 40)
			&& (nProDOSsector >= 0 && nProDOSsector < 16))
		{
			/**/ if (activeFloppy == 0) drive1Sector = nProDOSsector;
			else if (activeFloppy == 1) drive2Sector = nProDOSsector;

			isSectorValid = true;
		}
	}

	// Preserve sector so don't get "??" when switching drives, eg. if using COPYA to copy from drive-1 to drive-2
	if (isSectorValid)
	{
		if (activeFloppy == 0) g_nSector[slot][0] = drive1Sector;
		else                   g_nSector[slot][1] = drive2Sector;
	}
	else
	{
		if (activeFloppy == 0) g_nSector[slot][0] = -1;
		else                   g_nSector[slot][1] = -1;
	}
}

void Win32Frame::CreateTrackSectorStrings(int track, int sector, std::string& strTrack, std::string& strSector)
{
	strTrack = StrFormat("%2d", track);
	strSector = (sector < 0) ? "??" : StrFormat("%2d", sector);
}

void Win32Frame::DrawTrackSector(HDC dc, UINT slot, int drive1Track, int drive1Sector, int drive2Track, int drive2Sector)
{
	const int x = buttonx;
	const int y = buttony + BUTTONS * BUTTONCY + 1;

	const UINT yOffsetSlotNTrackInfo = (slot == SLOT6) ? yOffsetSlot6TrackInfo : yOffsetSlot5TrackInfo;
	const UINT yOffsetSlotNSectorInfo = yOffsetSlotNTrackInfo + smallfontHeight;

	// Erase background (TrackInfo + SectorInfo)
	SelectObject(dc, GetStockObject(NULL_PEN));
	SelectObject(dc, btnfacebrush);
	Rectangle(dc, x + 4, y + yOffsetSlotNTrackInfo, x + BUTTONCX + 1, y + yOffsetSlotNSectorInfo + smallfontHeight);

	SetTextColor(dc, RGB(0, 0, 0));
	SetBkMode(dc, TRANSPARENT);

	std::string strTrackDrive1, strSectorDrive1, strTrackDrive2, strSectorDrive2;
	CreateTrackSectorStrings(drive1Track, drive1Sector, strTrackDrive1, strSectorDrive1);
	CreateTrackSectorStrings(drive2Track, drive2Sector, strTrackDrive2, strSectorDrive2);

	std::string text;
	text = "T" + strTrackDrive1;
	TextOut(dc, x + 6, y + yOffsetSlotNTrackInfo, text.c_str(), text.length());
	text = "S" + strSectorDrive1;
	TextOut(dc, x + 6, y + yOffsetSlotNSectorInfo, text.c_str(), text.length());

	text = "T" + strTrackDrive2;
	TextOut(dc, x + 26, y + yOffsetSlotNTrackInfo, text.c_str(), text.length());
	text = "S" + strSectorDrive2;
	TextOut(dc, x + 26, y + yOffsetSlotNSectorInfo, text.c_str(), text.length());
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

	int nDrive1Track, nDrive2Track, nActiveFloppy;
	GetTrackSector(SLOT6, nDrive1Track, nDrive2Track, nActiveFloppy);

	// Draw Track/Sector
	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(g_hFrameWindow));
	const int x = buttonx;
	const int y = buttony + BUTTONS * BUTTONCY + 1;

	SelectObject(dc,smallfont);
	SetBkMode(dc,OPAQUE);
	SetBkColor(dc,RGB(0,0,0));
	SetTextAlign(dc,TA_LEFT | TA_TOP);

	if (g_bIsFullScreen)
	{
		// GH#57 - drive lights in full screen mode (Slot 6 only)

		if (!g_bFullScreen_ShowSubunitStatus)
			return;

		std::string strTrackDrive1, strSectorDrive1, strTrackDrive2, strSectorDrive2;
		CreateTrackSectorStrings(nDrive1Track, g_nSector[SLOT6][0], strTrackDrive1, strSectorDrive1);
		CreateTrackSectorStrings(nDrive2Track, g_nSector[SLOT6][1], strTrackDrive2, strSectorDrive2);

		std::string text = ( nActiveFloppy == 0 )
			? StrFormat( "%s/%s    ", strTrackDrive1.c_str(), strSectorDrive1.c_str() )
			: StrFormat( "%s/%s    ", strTrackDrive2.c_str(), strSectorDrive2.c_str() );

		SetTextColor(dc, g_aDiskFullScreenColorsLED[ DISK_STATUS_READ ] );
		TextOut(dc, x, y, text.c_str(), text.length());

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive1]);
		TextOut(dc, x + 3, y + smallfontHeight, TEXT("1"), 1);

		SetTextColor(dc, g_aDiskFullScreenColorsLED[g_eStatusDrive2]);
		TextOut(dc, x + 13, y + smallfontHeight, TEXT("2"), 1);
	}
	else
	{
		// NB. Only draw Track/Sector if 2x windowed
		if (g_nViewportScale == 1 || !GetWindowedModeShowDiskiiStatus())
			return;

		DrawTrackSector(dc, SLOT6, nDrive1Track, g_nSector[SLOT6][0], nDrive2Track, g_nSector[SLOT6][1]);

		// Slot 5's Disk II
		if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
		{
			GetTrackSector(SLOT5, nDrive1Track, nDrive2Track, nActiveFloppy);
			DrawTrackSector(dc, SLOT5, nDrive1Track, g_nSector[SLOT5][0], nDrive2Track, g_nSector[SLOT5][1]);
		}
	}
}

//===========================================================================
void Win32Frame::DrawStatusArea(HDC passdc, int drawflags)
{
	if (g_hFrameWindow == NULL)
	{
		// TC: Fix drawing of drive buttons before frame created:
		// . Main init loop: LoadConfiguration() called before FrameCreateWindow(), eg:
		//   LoadConfiguration() -> Disk_LoadLastDiskImage() -> DiskInsert() -> GetFrame().FrameRefreshStatus()
		return;
	}

	FrameReleaseDC();
	HDC  dc     = (passdc ? passdc : GetDC(g_hFrameWindow));
	const int x = buttonx;
	const int y = buttony + BUTTONS * BUTTONCY + 1;
	const bool bCaps = KeybGetCapsStatus();

	Disk_Status_e eHardDriveStatus = DISK_STATUS_OFF;
	if (GetCardMgr().QuerySlot(SLOT7) == CT_GenericHDD)
		dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(SLOT7)).GetLightStatus(&eHardDriveStatus);

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
			{
				FrameDrawDiskStatus(dc);
			}

			if (GetCardMgr().QuerySlot(SLOT7) == CT_GenericHDD)
			{
				SetTextAlign(dc, TA_RIGHT | TA_TOP);
				SetTextColor(dc, g_aDiskFullScreenColorsLED[eHardDriveStatus]);
				TextOut(dc, x + 23, y + 2, TEXT("H"), 1);
			}

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
			// Erase background (Slot6 drive LEDs, HDD LED & Caps)
			SelectObject(dc,GetStockObject(NULL_PEN));
			SelectObject(dc,btnfacebrush);
			Rectangle(dc,x,y,x+BUTTONCX+2,y+34);
			Draw3dRect(dc,x+1,y+3,x+BUTTONCX,y+30,0);

			// Add text for Slot6 drives: "1" & "2"
			SelectObject(dc,smallfont);
			SetTextAlign(dc,TA_CENTER | TA_TOP);
			SetTextColor(dc,RGB(0,0,0));
			SetBkMode(dc,TRANSPARENT);
			TextOut(dc, x + 7, y + yOffsetSlot6LEDNumbers, "1", 1);
			TextOut(dc, x + 27, y + yOffsetSlot6LEDNumbers, "2", 1);

			// Add text for Slot7 harddrive: "H"
			TextOut(dc, x + 7, y + yOffsetCapsLock, TEXT("H"), 1);

			if (g_nViewportScale > 1)
			{
				if (m_redrawDiskiiStatus)
				{
					m_redrawDiskiiStatus = false;

					// Erase background (Slot6's TrackInfo + SectorInfo + "Slot 5" + LEDs + TrackInfo + SectorInfo)
					SelectObject(dc, GetStockObject(NULL_PEN));
					SelectObject(dc, btnfacebrush);
					Rectangle(dc, x + 1, y + yOffsetSlot6TrackInfo, x + BUTTONCX + 1, y + yOffsetSlot5SectorInfo + smallfontHeight);
				}

				if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
				{
					std::string slot5 = "Slot 5:";
					TextOut(dc, x + 15, y + yOffsetSlot5Label, slot5.c_str(), slot5.length());
					TextOut(dc, x + 7, y + yOffsetSlot5LEDNumbers, "1", 1);
					TextOut(dc, x + 27, y + yOffsetSlot5LEDNumbers, "2", 1);
				}
			}
		}

		if (drawflags & DRAW_LEDS)
		{
			FrameDrawDiskLEDS( dc );

			if (drawflags & DRAW_DISK_STATUS)
				FrameDrawDiskStatus( dc );

			if (!IS_APPLE2)
			{
				RECT rCapsLed = {0,0,10,12}; // HACK: HARD-CODED bitmaps size
				switch (g_Apple2Type)
				{
					case A2TYPE_APPLE2:
					case A2TYPE_APPLE2PLUS:
					case A2TYPE_APPLE2E:
					case A2TYPE_APPLE2EENHANCED:
					default: DrawBitmapRect(dc, x + 31, y + yOffsetCapsLock, &rCapsLed, g_hCapsLockBitmap[bCaps != 0]); break;
					case A2TYPE_PRAVETS82:
					case A2TYPE_PRAVETS8M: DrawBitmapRect(dc, x + 31, y + yOffsetCapsLock, &rCapsLed, g_hCapsBitmapP8[bCaps != 0]); break; // TODO: FIXME: Shouldn't one of these use g_hCapsBitmapLat ??
					case A2TYPE_PRAVETS8A: DrawBitmapRect(dc, x + 31, y + yOffsetCapsLock, &rCapsLed, g_hCapsBitmapP8[bCaps != 0]); break;
				}

				RECT rDiskLed = {0,0,8,8};
				DrawBitmapRect(dc, x + 12, y + yOffsetHardDiskLED, &rDiskLed, g_hDiskWindowedLED[eHardDriveStatus]);
			}
		}

		if (drawflags & DRAW_TITLE)
		{
			GetAppleWindowTitle(); // SetWindowText() // WindowTitle
			SendMessage(g_hFrameWindow,WM_SETTEXT,0,(LPARAM)g_pAppTitle.c_str());
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
void Win32Frame::EraseButton (int number) {
  RECT rect;
  rect.left   = buttonx;
  rect.right  = rect.left+BUTTONCX;
  rect.top    = buttony+number*BUTTONCY;
  rect.bottom = rect.top+BUTTONCY;

	InvalidateRect(g_hFrameWindow,&rect,1);
}

//===========================================================================

LRESULT CALLBACK Win32Frame::FrameWndProc(
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam)
{
	Win32Frame& win32Frame = Win32Frame::GetWin32Frame();
	return win32Frame.WndProc(window, message, wparam, lparam);
}

LRESULT Win32Frame::WndProc(
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
        BOOL bRes = KillTimer(g_hFrameWindow, g_TimerIDEvent_100msec);
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
	  GetCardMgr().Destroy();
      CpuDestroy();
      MemDestroy();
      SpkrDestroy();
      Destroy();
      DeleteGdiObjects();
      DIMouse::DirectInputUninit(window);	// NB. do before window is destroyed
      PostQuitMessage(0);	// Post WM_QUIT message to the thread's message queue
      g_hFrameWindow = (HWND)0;
      LogFileOutput("WM_DESTROY (done)\n");
      break;

    case WM_CREATE:
      LogFileOutput("WM_CREATE\n");
      g_hFrameWindow = window;	// NB. g_hFrameWindow by CreateWindow()

      CreateGdiObjects();
      LogFileOutput("WM_CREATE: CreateGdiObjects()\n");

	  DSInit();					// NB. Need g_hFrameWindow for IDirectSound::SetCooperativeLevel()
      LogFileOutput("WM_CREATE: DSInit()\n");

	  DIMouse::DirectInputInit(window);
      LogFileOutput("WM_CREATE: DIMouse::DirectInputInit()\n");

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
      GetVideo().VideoReinitialize(false);
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
			Video_TakeScreenShot( Video::SCREENSHOT_560x384 );
		}
		else
		if (wparam == VK_SNAPSHOT_280) // ( lparam & MOD_SHIFT )
		{
			Video_TakeScreenShot( Video::SCREENSHOT_280x192 );
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
			ApplyVideoModeChange();
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
				GetPravets().ToggleP8ACapsLock();	// F10: Toggles Pravets8A Capslock
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
				VideoRedrawScreen();
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
				KeybUpdateCtrlShiftStatus(); // #1364
				POINT pt;		// location of mouse click
				pt.x = buttonx + BUTTONCX/2;
				pt.y = buttony + BUTTONCY/2 + iButton * BUTTONCY;
				const int iDrive = wparam - VK_F3;
				ProcessDiskPopupMenu( window, pt, iDrive );

				FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
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
						ScreenToClient(g_hFrameWindow, &Point);
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
			if (pInfo->hdr.idFrom == TTID_DRIVE1_BUTTON || pInfo->hdr.idFrom == TTID_DRIVE2_BUTTON)
			{
				SendMessage(pInfo->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 150);

				Disk2InterfaceCard* pDisk2Slot5 = NULL, * pDisk2Slot6 = NULL;

				if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
					pDisk2Slot5 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT5));
				if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
					pDisk2Slot6 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT6));

				std::string slot5 = pDisk2Slot5 ? pDisk2Slot5->GetFullDiskFilename(pInfo->hdr.idFrom) : "";
				std::string slot6 = pDisk2Slot6 ? pDisk2Slot6->GetFullDiskFilename(pInfo->hdr.idFrom) : "";

				if (pDisk2Slot5)
				{
					if (slot6.empty()) slot6 = "<empty>";
					if (slot5.empty()) slot5 = "<empty>";
					slot6 = std::string("Slot6: ") + slot6;
					slot5 = std::string("Slot5: ") + slot5;
				}

				std::string join = (!slot6.empty() && !slot5.empty()) ? "\n" : "";
				driveTooltip = slot6 + join + slot5;
				pInfo->lpszText = (LPTSTR)driveTooltip.c_str();
			}
			else if (pInfo->hdr.idFrom == TTID_SLOT6_TRK_SEC_INFO || pInfo->hdr.idFrom == TTID_SLOT5_TRK_SEC_INFO)
			{
				if (!GetWindowedModeShowDiskiiStatus())
					break;

				SendMessage(pInfo->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 200);

				const UINT slot = (pInfo->hdr.idFrom == TTID_SLOT6_TRK_SEC_INFO) ? SLOT6 : SLOT5;
				Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));
				float drive1Track = disk2Card.GetPhase(DRIVE_1) / 2;
				float drive2Track = disk2Card.GetPhase(DRIVE_2) / 2;

				// Example tooltip:
				//
				//   Drive 1:          Drive 2:
				//   T$10.00 (T16.00)  T$0C.00 (T12.00)
				//   S$06 (S06)        S$0F (S15)
				//
				driveTooltip = "Drive 1:\t\tDrive 2:\n";
				driveTooltip += "T$";
				driveTooltip += disk2Card.FormatIntFracString(drive1Track, true);
				driveTooltip += " (T";
				driveTooltip += disk2Card.FormatIntFracString(drive1Track, false);
				driveTooltip += ")";
				driveTooltip += "\tT$";
				driveTooltip += disk2Card.FormatIntFracString(drive2Track, true);
				driveTooltip += " (T";
				driveTooltip += disk2Card.FormatIntFracString(drive2Track, false);
				driveTooltip += ")";
				driveTooltip += "\n";
				// hex sector
				driveTooltip += "S$";
				char sector[3] = "??";
				if (g_nSector[slot][0] >= 0) sprintf_s(sector, "%02X", g_nSector[slot][0]);
				driveTooltip += sector;
				// dec sector
				driveTooltip += " (S";
				strcpy(sector, "??");
				if (g_nSector[slot][0] >= 0) sprintf_s(sector, "%02d", g_nSector[slot][0]);
				driveTooltip += sector;
				driveTooltip += ")";
				// hex sector
				driveTooltip += "\tS$";
				strcpy(sector, "??");
				if (g_nSector[slot][1] >= 0) sprintf_s(sector, "%02X", g_nSector[slot][1]);
				driveTooltip += sector;
				// dec sector
				driveTooltip += " (S";
				strcpy(sector, "??");
				if (g_nSector[slot][1] >= 0) sprintf_s(sector, "%02d", g_nSector[slot][1]);
				driveTooltip += sector;
				driveTooltip += ")";

				pInfo->lpszText = (LPTSTR)driveTooltip.c_str();
			}
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

					FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
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
      Win32Frame::GetWin32Frame().Benchmark();
      g_nAppMode = MODE_LOGO;
      ResetMachineState();
      SetCursor(oldcursor);
      break;
    }

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
void Win32Frame::ScreenWindowResize(const bool bCtrlKey)
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

bool Win32Frame::ConfirmReboot(bool bFromButtonUI)
{
	if (!bFromButtonUI || !g_bConfirmReboot)
		return true;

	int res = FrameMessageBox(
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

void Win32Frame::ProcessButtonClick(int button, bool bFromButtonUI /*=false*/)
{
	SoundCore_SetFade(FADE_OUT);
	bool bAllowFadeIn = true;

#if DEBUG_DD_PALETTE
	LogOutput( "Button: F%d  Full Screen: %d\n", button+1, g_bIsFullScreen );
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

        HtmlHelp(g_hFrameWindow,filename.c_str(),HH_DISPLAY_TOC,0);
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
      VideoRedrawScreen();
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
			GetVideo().ClearSHRResidue();	// Clear the framebuffer to remove any SHR residue in the borders
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
inline int Util_GetTrackSectorOffset( const int nTrack, const int nSector )
{
	return (TRACK_DENIBBLIZED_SIZE * nTrack) + (nSector * 256);
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

	enum { DSK_SECTOR_SIZE = 256 };

	enum SectorOrder_e
	{
		  INTERLEAVE_AUTO_DETECT
		, INTERLEAVE_DOS33_ORDER
		, INTERLEAVE_PRODOS_ORDER
	};

	// Map Physical <-> Logical
	const uint8_t g_aInterleave_DSK[ 16 ] =
	{
		//0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F // logical order
		0x0,0xE,0xD,0xC,0xB,0xA,0x9,0x8,0x7,0x6,0x5,0x4,0x3,0x2,0x1,0xF // physical order
	};

	void Util_Disk_InterleaveForward( int iSector, size_t *src_, size_t *dst_ )
	{
		*src_ = g_aInterleave_DSK[ iSector ]*DSK_SECTOR_SIZE;
		*dst_ =                    iSector  *DSK_SECTOR_SIZE; // linearize
	};

	void Util_Disk_InterleaveReverse( int iSector, size_t *src_, size_t *dst_ )
	{
		*src_ =                    iSector  *DSK_SECTOR_SIZE; // un-linearize
		*dst_ = g_aInterleave_DSK[ iSector ]*DSK_SECTOR_SIZE;
	}

	// With C++17 could use std::filesystem::path
	// returns '.dsk'
	// Alt. PathFindExtensionA()
	std::string Util_GetFileNameExtension(const std::string& pathname)
	{
#if _MSVC_LANG >= 201703L // Compiler option: /std:c++17
		const std::filesystem::path path( pathname );
		return path.extension().generic_string();
#else
		const size_t nLength    = pathname.length();
		const size_t iExtension = pathname.rfind( '.', nLength );
		if (iExtension != std::string::npos)
			return pathname.substr( iExtension, nLength );
		else
			return std::string("");
#endif
	}

	SectorOrder_e Util_Disk_CalculateSectorOrder(const std::string& pathname)
	{
		SectorOrder_e eOrder = INTERLEAVE_AUTO_DETECT;
		std::string sExtension = Util_GetFileNameExtension( pathname );
		if (sExtension.length())
		{
			/**/ if (sExtension == ".dsk") eOrder = INTERLEAVE_DOS33_ORDER;
			else if (sExtension == ".do" ) eOrder = INTERLEAVE_DOS33_ORDER;
			else if (sExtension == ".po" ) eOrder = INTERLEAVE_PRODOS_ORDER;
			else if (sExtension == ".hdv") eOrder = INTERLEAVE_PRODOS_ORDER;
		}
		return eOrder;
	}

// Swizzle sectors in DOS33 order to ProDOS order in-place
//===========================================================================
void Util_ProDOS_ForwardSectorInterleave (uint8_t *pDiskBytes, const size_t nDiskSize, const SectorOrder_e eSectorOrder)
{
	if (eSectorOrder == INTERLEAVE_DOS33_ORDER)
	{
		size_t   nOffset = 0;
		uint8_t *pSource = new uint8_t[ nDiskSize ];
		memcpy( pSource, pDiskBytes, nDiskSize );

		const int nTracks = nDiskSize / TRACK_DENIBBLIZED_SIZE;
		for( int iTrack = 0; iTrack < nTracks; iTrack++ )
		{
			for( int iSector = 0; iSector < 16; iSector++ )
			{
				size_t nSrc;
				size_t nDst;
				Util_Disk_InterleaveForward( iSector, &nSrc, &nDst );
				memcpy( pDiskBytes + nOffset + nDst, pSource + nOffset + nSrc, DSK_SECTOR_SIZE );
			}
			nOffset += TRACK_DENIBBLIZED_SIZE;
		}

		delete [] pSource;
	}
}

// See:
// * https://prodos8.com/docs/technote/30/
// * 2.2.3 Sparse Files
//   https://prodos8.com/docs/techref/file-use/
// * B.3.6 - Sparse Files
//   https://prodos8.com/docs/techref/file-organization/
//===========================================================================
bool Util_ProDOS_IsFileBlockSparse( int nOffset, const uint8_t* pFileData, const size_t nFileSize )
{
	if ((size_t)nOffset >= nFileSize)
		return false;

	const int nEnd   = (nOffset + PRODOS_BLOCK_SIZE ) > nFileSize
		? nOffset + (nFileSize % PRODOS_BLOCK_SIZE)
		: nOffset +              PRODOS_BLOCK_SIZE;
	for (int iOffset = nOffset ; iOffset < nEnd; iOffset++ )
	{
		if (pFileData[ iOffset ])
		{
			return false;
		}
	}
	return true;
}

// Swizzles sectors in ProDOS order to DOS33 order in-place
//===========================================================================
void Util_ProDOS_ReverseSectorInterleave (uint8_t *pDiskBytes, const size_t nDiskSize, const SectorOrder_e eSectorOrder)
{
	// Swizle from ProDOS to DOS33 order
	if (eSectorOrder == INTERLEAVE_DOS33_ORDER)
	{
		size_t   nOffset = 0;
		uint8_t *pSource = new uint8_t[ nDiskSize ];
		memcpy( pSource, pDiskBytes, nDiskSize );

		const int nTracks = nDiskSize / TRACK_DENIBBLIZED_SIZE;
		for( int iTrack = 0; iTrack < nTracks; iTrack++ )
		{
			for( int iSector = 0; iSector < 16; iSector++ )
			{
				size_t nSrc;
				size_t nDst;
				Util_Disk_InterleaveReverse( iSector, &nSrc, &nDst );
				memcpy( pDiskBytes + nOffset + nDst, pSource + nOffset + nSrc, DSK_SECTOR_SIZE );
			}
			nOffset += TRACK_DENIBBLIZED_SIZE;
		}

		delete [] pSource;
	}
}

// NB Assumes pDiskBytes are in ProDOS order!
//===========================================================================
void Util_ProDOS_FormatFileSystem (uint8_t *pDiskBytes, const size_t nDiskSize, const char *pVolumeName)
{
	const size_t nBootBlockSize = DSK_SECTOR_SIZE * 2;
	memset( pDiskBytes + nBootBlockSize, 0, nDiskSize - nBootBlockSize );

	// Create blocks for root directory
	int nRootDirBlocks = 4;
	int iPrevDirBlock  = 0;
	int iNextDirBlock  = 0;
	int iOffset;

	ProDOS_VolumeHeader_t  tVolume;
	ProDOS_VolumeHeader_t *pVolume = &tVolume;
	memset( pVolume, 0, sizeof(ProDOS_VolumeHeader_t) );

	// Init Bitmap
	pVolume->meta.bitmap_block = (uint16_t) (PRODOS_ROOT_BLOCK + nRootDirBlocks);
	int nBitmapBlocks = ProDOS_BlockInitFree( pDiskBytes, nDiskSize, pVolume );

	// Set boot blocks as in-use
	for( int iBlock = 0; iBlock < PRODOS_ROOT_BLOCK; iBlock++ )
		ProDOS_BlockSetUsed( pDiskBytes, pVolume, iBlock );

	for( int iBlock = 0; iBlock < nRootDirBlocks; iBlock++ )
	{
		iNextDirBlock = ProDOS_BlockGetFirstFree( pDiskBytes, nDiskSize, pVolume );
		iOffset       = iNextDirBlock * PRODOS_BLOCK_SIZE;
		ProDOS_BlockSetUsed( pDiskBytes, pVolume, iNextDirBlock );

		// Double Linked List
		// [0] = prev
		// [2] = next -- will be set on next allocation
		ProDOS_Put16( pDiskBytes, iOffset + 0, iPrevDirBlock );
		ProDOS_Put16( pDiskBytes, iOffset + 2, 0 );

		if( iBlock )
		{
			// Fixup previous directory block with pointer to this one
			iOffset = iPrevDirBlock * PRODOS_BLOCK_SIZE;
			ProDOS_Put16( pDiskBytes, iOffset + 2, iNextDirBlock );
		}

		iPrevDirBlock = iNextDirBlock;
	}

	// Alloc Bitmap Blocks
	for( int iBlock = 0; iBlock < nBitmapBlocks; iBlock++ )
	{
		int iBitmap = ProDOS_BlockGetFirstFree( pDiskBytes, nDiskSize, pVolume );
		ProDOS_BlockSetUsed( pDiskBytes, pVolume, iBitmap );
	}

	pVolume->entry_len  = 0x27;
	pVolume->entry_num  = (uint8_t) (PRODOS_BLOCK_SIZE / pVolume->entry_len);
	pVolume->file_count = 0;

	if( *pVolumeName == '/' )
		pVolumeName++;

	size_t nLen = strlen( pVolumeName );

	pVolume->kind = PRODOS_KIND_ROOT;
	pVolume->len  = (uint8_t) nLen;
	ProDOS_String_CopyUpper( pVolume->name, pVolumeName, 15 );

	pVolume->access = 0
		| ACCESS_D
		| ACCESS_N
		| ACCESS_B
		| ACCESS_W
		| ACCESS_R
		;

	// TODO:
	//tVolume.date   = config.date;
	//tVolume.time   = config.time;

	ProDOS_SetVolumeHeader( pDiskBytes, pVolume, PRODOS_ROOT_BLOCK );
}

//===========================================================================
bool Util_ProDOS_AddFile (uint8_t* pDiskBytes, const size_t nDiskSize, const char* pVolumeName, const uint8_t* pFileData, const size_t nFileSize, ProDOS_FileHeader_t &meta, const bool bAllowSparseFile = false)
{
	assert( pFileData );

	int iBase = ProDOS_BlockGetPathOffset( pDiskBytes, nullptr, "/" ); // On an empty disk this will be PRODOS_ROOT_OFFSET
	int iDirBlock = iBase / PRODOS_BLOCK_SIZE;

	ProDOS_VolumeHeader_t  tVolume;
	ProDOS_VolumeHeader_t *pVolume = &tVolume;
	memset( pVolume, 0, sizeof(ProDOS_VolumeHeader_t) );

	ProDOS_GetVolumeHeader( pDiskBytes, pVolume, iDirBlock );

	// Verify we have room in the current directory
	int iFreeOffset   = ProDOS_DirGetFirstFreeEntryOffset( pDiskBytes, pVolume, iBase );
	if (iFreeOffset <= 0)
		return false; // disk full

	int iKind        = PRODOS_KIND_DEL;
	int nBlocksData  = (nFileSize + PRODOS_BLOCK_SIZE - 1) / PRODOS_BLOCK_SIZE;
	int nBlocksIndex = 0; // Num Blocks needed for meta (Index)
	int nBlocksTotal = 0;
	int nBlocksFree  = 0;
	int iNode        = 0; // Master Index, Single Index, or 0 if none
	int iIndexBase   = 0; // Single Index
	int iMasterIndex = 0; // master block points to N IndexBlocks

	// Calculate size of meta blocks and kind of file
	if (nFileSize <=   1*PRODOS_BLOCK_SIZE) // <= 512, 1 Block
	{
		iKind = PRODOS_KIND_SEED;
	}
	else
	if (nFileSize > 256*PRODOS_BLOCK_SIZE) // >= 128K, 257-65536 Blocks
	{
		iKind        = PRODOS_KIND_TREE;
		nBlocksIndex = (nBlocksData + (PRODOS_BLOCK_SIZE/2-1)) / (PRODOS_BLOCK_SIZE / 2);
		nBlocksIndex++; // include master index block
	}
	else
	if( nFileSize > PRODOS_BLOCK_SIZE ) // <= 128K, 2-256 blocks
	{
		iKind        = PRODOS_KIND_SAPL;
		nBlocksIndex = 1; // single index = PRODOS_BLOCK_SIZE/2 = 256 data blocks
	}

	// We simply can't set nBlocksTotal
	//     nBlocksTotal = nBlocksIndex + nBlocksData;
	// Since we may have sparse blocks
	nBlocksTotal = nBlocksIndex;

	for( int iBlock = 0; iBlock < nBlocksIndex; iBlock++ )
	{
		// Blank Volume
		//  0 ] Boot Apple //e
		//  1 ] Boot Apple ///
		//  2 \ Root Directory
		//  3 |
		//  4 |
		//  5 /
		//  6   Volume Bitmap
		//  7   First Free Block
		int iMetaBlock = ProDOS_BlockGetFirstFree( pDiskBytes, nDiskSize, pVolume );
		if( !iMetaBlock )
		{
			return false; // out of disk space, no free room
		}

		// First Block has meta information -- blocks used for file
		if (iBlock == 0)
		{
			iNode      = iMetaBlock;
			iIndexBase = iMetaBlock * PRODOS_BLOCK_SIZE;
		}
		else
		{
			// PRODOS_KIND_SEED doesn't have an index block
			if (iKind == PRODOS_KIND_TREE)
			{
				ProDOS_PutIndexBlock( pDiskBytes, iMasterIndex, iBlock, iMetaBlock );
			}
			// Not implemented PRODOS_KIND_TREE
			assert( iKind != PRODOS_KIND_TREE );
		}

		ProDOS_BlockSetUsed( pDiskBytes, pVolume, iMetaBlock );
#if _DEBUG
	LogOutput( "0x----  FileBlock: --/--  MetaBlock: $%02X\n", iMetaBlock );
#endif
	}

	// Copy Data
	const uint8_t *pSrc           = pFileData;
	const size_t   nSlack         = nFileSize % PRODOS_BLOCK_SIZE;
	const size_t   nLastBlockSize = nSlack ? nSlack : PRODOS_BLOCK_SIZE;
	const bool PRODOS_FILE_TYPE_TREE_NOT_IMPLEMENTED = false;

	for( int iBlock = 0; iBlock < nBlocksData; iBlock++ )
	{
		int  iDataBlock = ProDOS_BlockGetFirstFree( pDiskBytes, nDiskSize, pVolume );
		if( !iDataBlock )
		{
			return false;
		}

		// if file size <= 512 bytes
		if (iBlock == 0)
		{
			if (iKind == PRODOS_KIND_SEED)
			{
				iNode = iDataBlock;
			}
		}

		int      iDstOffset = iDataBlock * PRODOS_BLOCK_SIZE;
		uint8_t *pDst       = pDiskBytes + iDstOffset;
		bool     bLastBlock = iBlock == (nBlocksData - 1);

		// Any 512-byte block containing all zeroes doesn't need to be written to disk.
		// Instead we write an index block of $0000 to tell ProDOS that this block is a sparse block.
		bool bIsSparseBlock = Util_ProDOS_IsFileBlockSparse( iBlock * PRODOS_BLOCK_SIZE, pFileData, nFileSize );

#if _DEBUG
	LogOutput( "0x%04X  FileBlock: %2d/%2d  DataBlock: $%02X  LastBlock=%d  Sparse=%d\n"
		, iBlock*PRODOS_BLOCK_SIZE
		, iBlock+1
		, nBlocksData
		, iDataBlock
		, bLastBlock
		, bIsSparseBlock
	);
#endif
		if (bIsSparseBlock && bAllowSparseFile)
		{
			if (iKind == PRODOS_KIND_SAPL)
			{
				ProDOS_PutIndexBlock( pDiskBytes, iIndexBase, iBlock, 0 ); // Update File Bitmap
			}
			else
			if (iKind == PRODOS_KIND_TREE)
			{
				assert( PRODOS_FILE_TYPE_TREE_NOT_IMPLEMENTED ); // TODO: ProDOS tree file not implemented
			}
		}
		else
		{
			if (bLastBlock)
			{
				if (nLastBlockSize)
				{
					memcpy( pDst, pSrc, nLastBlockSize );
					nBlocksTotal++;
					ProDOS_BlockSetUsed( pDiskBytes, pVolume, iDataBlock ); // Update Volume Bitmap
				}
				else
				{
					iIndexBase = 0; // Last block has zero size. Don't update index block
				}
			}
			else
			{
				memcpy( pDst, pSrc, PRODOS_BLOCK_SIZE );
				nBlocksTotal++;
				ProDOS_BlockSetUsed( pDiskBytes, pVolume, iDataBlock ); // Update Volume Bitmap
			}

			if (iKind == PRODOS_KIND_SAPL)
			{
				// Update single index block
				if (iIndexBase)
				{
					ProDOS_PutIndexBlock( pDiskBytes, iIndexBase, iBlock, iDataBlock ); // Update File Bitmap
				}
			}
			else
			if (iKind == PRODOS_KIND_TREE)
			{
				// Update multiple index blocks
				assert( PRODOS_FILE_TYPE_TREE_NOT_IMPLEMENTED ); // TODO: ProDOS tree file not implemented
			}
		}
		pSrc += PRODOS_BLOCK_SIZE;
	}

	// Update directory entry with meta information
	meta.inode     = iNode;
	meta.blocks    = nBlocksTotal; // Note: File Entry DOES include index + non-sparse data block(s)
	meta.dir_block = iDirBlock;
	ProDOS_PutFileHeader( pDiskBytes, iFreeOffset, &meta );

	// Update Directory with Header
	pVolume->file_count++;
	ProDOS_SetVolumeHeader( pDiskBytes, pVolume, iDirBlock );

	return true;
}

//===========================================================================
bool Util_ProDOS_CopyBASIC ( uint8_t *pDiskBytes, const size_t nDiskSize, const char *pVolumeName, Win32Frame *pWinFrame )
{
	const size_t   nFileSize = 0x2800; // 10,240 -> 20 blocks + 1 index block = 21 blocks
	const uint8_t *pFileData = (uint8_t *) pWinFrame->GetResource(IDR_FILE_BASIC17, "FIRMWARE", nFileSize);

	// Acc dnb??iwr /PRODOS.2.4.3    Blocks Size    Type    Aux   Kind  iNode Dir   Ver Min  Create    Time    Modified  Time
	// --- -------- ---------------- ------ ------- ------- ----- ----- ----- ----- --- ---  --------- ------  --------- ------
	// $21 --b----r *BASIC.SYSTEM        21 $002800 SYS $FF $2000 sap 2 @0029 @0002 2.4 v00  13-JAN-18  9:09a  30-AUG-16  7:56a
	int bAccess = 0
		| ACCESS_B
		| ACCESS_R
		;
	ProDOS_FileHeader_t meta;
	memset( &meta, 0, sizeof(ProDOS_FileHeader_t) );

	const char    *pName = "BASIC.SYSTEM";
	const uint16_t nDateCreate = ProDOS_PackDate( 18, 1, 13 ); // YYMMDD, NOTE: Jan starts at 1, not 0.
	const uint16_t nTimeCreate = ProDOS_PackTime( 9, 9 );
	const uint16_t nDateModify = ProDOS_PackDate( 16, 8, 30 ); // YYMMDD
	const uint16_t nTimeModify = ProDOS_PackTime( 7, 56 );

	meta.kind      = PRODOS_KIND_SAPL;
	strcpy( meta.name, pName );
	meta.len       = strlen( pName ) & 0xF;
	meta.type      = 0xFF; // SYS
	//  .inode     = TBD
	//  .blocks    = TBD
	meta.size      = nFileSize;
	meta.date      = nDateCreate;
	meta.time      = nTimeCreate;
	meta.cur_ver   = 0x24;
	meta.min_ver   = 0x00;
	meta.access    = bAccess;
	meta.aux       = 0x2000;
	meta.mod_date  = nDateModify;
	meta.mod_time  = nTimeModify;
	//  .dir_block = TBD;

	return Util_ProDOS_AddFile( pDiskBytes, nDiskSize, pVolumeName, pFileData, nFileSize, meta, true );
}

//===========================================================================
bool Util_ProDOS_CopyBitsyBoot ( uint8_t *pDiskBytes, const size_t nDiskSize, const char *pVolumeName, Win32Frame *pWinFrame )
{
	const size_t   nFileSize = 0x16D; // < 512 bytes -> SEED, only 1 data block
	const uint8_t *pFileData = (uint8_t *) pWinFrame->GetResource(IDR_FILE_BITSY_BOOT, "FIRMWARE", nFileSize);

	// Acc dnb??iwr /PRODOS.2.4.2    Blocks Size    Type    Aux   Kind  iNode Dir   Ver Min  Create    Time    Modified  Time
	// --- -------- ---------------- ------ ------- ------- ----- ----- ----- ----- --- ---  --------- ------  --------- ------
	// $21 --b----r *BITSY.BOOT           1 $00016D SYS $FF $2000 sed 1 @003D @0002 2.4 v00  13-JAN-18  9:09a  15-SEP-16  9:49a
	int bAccess = 0
		| ACCESS_B
		| ACCESS_R
		;

	ProDOS_FileHeader_t meta;
	memset( &meta, 0, sizeof(ProDOS_FileHeader_t) );

	const char    *pName = "BITSY.BOOT";
	const uint16_t nDateCreate = ProDOS_PackDate( 18, 1, 13 ); // YYMMDD, NOTE: Jan starts at 1, not 0.
	const uint16_t nTimeCreate = ProDOS_PackTime( 9, 9 );
	const uint16_t nDateModify = ProDOS_PackDate( 16, 9, 15 ); // YYMMDD
	const uint16_t nTimeModify = ProDOS_PackTime( 9, 49 );

	meta.kind      = PRODOS_KIND_SEED; // File size is <= 512 bytes
	strcpy( meta.name, pName );
	meta.len       = strlen( pName ) & 0xF;
	meta.type      = 0xFF; // SYS
	//  .inode     = TBD
	//  .blocks    = TBD
	meta.size      = nFileSize;
	meta.date      = nDateCreate;
	meta.time      = nTimeCreate;
	meta.cur_ver   = 0x24;
	meta.min_ver   = 0x00;
	meta.access    = bAccess;
	meta.aux       = 0x2000;
	meta.mod_date  = nDateModify;
	meta.mod_time  = nTimeModify;
	//  .dir_block = TBD;

	return Util_ProDOS_AddFile( pDiskBytes, nDiskSize, pVolumeName, pFileData, nFileSize, meta );
}

//===========================================================================
bool Util_ProDOS_CopyBitsyBye ( uint8_t *pDiskBytes, const size_t nDiskSize, const char *pVolumeName, Win32Frame *pWinFrame )
{
	const size_t   nFileSize = 0x0038; // < 512 bytes -> SEED, only 1 data block
	const uint8_t *pFileData = (uint8_t*) pWinFrame->GetResource(IDR_FILE_BITSY_BYE, "FIRMWARE", nFileSize);

	// Acc dnb??iwr /PRODOS.2.4.2    Blocks Size    Type    Aux   Kind  iNode Dir   Ver Min  Create    Time    Modified  Time
	// --- -------- ---------------- ------ ------- ------- ----- ----- ----- ----- --- ---  --------- ------  --------- ------
	// $21 --b----r *QUIT.SYSTEM          1 $000038 SYS $FF $2000 sed 1 @0027 @0002 2.4 v00  13-JAN-18  9:09a  15-SEP-16  9:41a
	int bAccess = 0
		| ACCESS_B
		| ACCESS_R
		;

	ProDOS_FileHeader_t meta;
	memset( &meta, 0, sizeof(ProDOS_FileHeader_t) );

	const char    *pName = "QUIT.SYSTEM";
	const uint16_t nDateCreate = ProDOS_PackDate( 18, 1, 13 ); // YYMMDD, NOTE: Jan starts at 1, not 0.
	const uint16_t nTimeCreate = ProDOS_PackTime( 9, 9 );
	const uint16_t nDateModify = ProDOS_PackDate( 16, 9, 15 ); // YYMMDD
	const uint16_t nTimeModify = ProDOS_PackTime( 9, 41 );

	meta.kind      = PRODOS_KIND_SEED; // File size is <= 512 bytes
	strcpy( meta.name, pName );
	meta.len       = strlen( pName ) & 0xF;
	meta.type      = 0xFF; // SYS
	//  .inode     = TBD
	//  .blocks    = TBD
	meta.size      = nFileSize;
	meta.date      = nDateCreate;
	meta.time      = nTimeCreate;
	meta.cur_ver   = 0x24;
	meta.min_ver   = 0x00;
	meta.access    = bAccess;
	meta.aux       = 0x2000;
	meta.mod_date  = nDateModify;
	meta.mod_time  = nTimeModify;
	//  .dir_block = TBD;

	return Util_ProDOS_AddFile( pDiskBytes, nDiskSize, pVolumeName, pFileData, nFileSize, meta );
}

//===========================================================================
bool Util_ProDOS_CopyDOS ( uint8_t *pDiskBytes, const size_t nDiskSize, const char *pVolumeName, Win32Frame *pWinFrame )
{
	const size_t   nFileSize = 0x42E8; // 17,128 -> 34 blocks but last block is sparse -> 33 data blocks + 1 index block
	const uint8_t *pFileData = (uint8_t*) pWinFrame->GetResource(IDR_OS_PRODOS243, "FIRMWARE", nFileSize);

	// Acc dnb??iwr /PRODOS.2.4.3    Blocks Size    Type    Aux   Kind  iNode Dir   Ver Min  Create    Time    Modified  Time
	// --- -------- ---------------- ------ ------- ------- ----- ----- ----- ----- --- ---  --------- ------  --------- ------
	// $E3 dnb---wr  PRODOS              34 $0042E8 SYS $FF $0000 sap 2 @0007 @0002 0.0 v80  30-DEC-23  2:43a  30-DEC-23  2:43a
	int bAccess = 0
		| ACCESS_D
		| ACCESS_N
		| ACCESS_B
		| ACCESS_W
		| ACCESS_R
		;
	ProDOS_FileHeader_t meta;
	memset( &meta, 0, sizeof(ProDOS_FileHeader_t) );

	const char *pName = "PRODOS";
	const uint16_t nDateCreate = ProDOS_PackDate( 23, 12, 30 ); // NOTE: Jan starts at 1, not 0.
	const uint16_t nTimeCreate = ProDOS_PackTime( 2, 43 ); // Version.  "Cute".
	const uint16_t nDateModify = ProDOS_PackDate( 23, 12, 30 );
	const uint16_t nTimeModify = ProDOS_PackTime( 2, 43 );

	meta.kind      = PRODOS_KIND_SAPL;
	strcpy( meta.name, pName );
	meta.len       = strlen( pName ) & 0xF;
	meta.type      = 0xFF; // SYS
	//  .inode     = TBD
	//  .blocks    = TBD
	meta.size      = nFileSize;
	meta.date      = nDateCreate;
	meta.time      = nTimeCreate;
	meta.cur_ver   = 0x00;
	meta.min_ver   = 0x80;
	meta.access    = bAccess;
	meta.aux       = 0x0000; // ignored for SYS, since load address = $2000
	meta.mod_date  = nDateModify;
	meta.mod_time  = nTimeModify;
	//  .dir_block = TBD;

	return Util_ProDOS_AddFile( pDiskBytes, nDiskSize, pVolumeName, pFileData, nFileSize, meta, true );
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// bSectorsUsed is 16-bits bitmask of sectors.
// 1 = Free
// 0 = Used
//===========================================================================
void Util_DOS33_SetTrackSectorUsage ( uint8_t *pVTOC, const int nTrack, const uint16_t bSectorsFree )
{
	int nOffset = 0x38 + (nTrack * 4);

	// Byte  Sectors
	//    0  FEDC BA98
	//    1  7654 3210
	//    2  -Wasted-
	//    3  -Wasted-
#if _DEBUG
	std::string sFree;
	std::string sUsed;
	for( int iSector = 15; iSector >= 0; iSector-- )
	{
		bool bFree = (bSectorsFree >> iSector) & 1;
		sFree.append( 1, (char)( bFree) | '0' );
		sUsed.append( 1, (char)(!bFree) ^ '0' );
		if ((iSector & 3) == 0)
		{
			sFree.append( 1, ' ' );
			sUsed.append( 1, ' ' );
		}
	}
	LogOutput("Track: %02X, VTOC[ %02X ]: %02X %02X %02X %02X -> %02X %02X,  Free: %s, Used: %s\n"
		, nTrack
		, nOffset
		, pVTOC[ nOffset + 0 ]
		, pVTOC[ nOffset + 1 ]
		, pVTOC[ nOffset + 2 ]
		, pVTOC[ nOffset + 3 ]
		, (bSectorsFree >> 8) & 0xFF
		, (bSectorsFree >> 0) & 0xFF
		, sFree.c_str()
		, sUsed.c_str()
	);
#endif
	pVTOC[ nOffset + 0 ] = (bSectorsFree >> 8) & 0xFF;
	pVTOC[ nOffset + 1 ] = (bSectorsFree >> 0) & 0xFF;
	pVTOC[ nOffset + 2 ] = 0x00;
	pVTOC[ nOffset + 3 ] = 0x00;
}

//===========================================================================
void Util_DOS33_SetTrackUsed ( uint8_t *pDiskBytes, const int nVTOC_Track, int nTrackUsed )
{
	int nOffset = Util_GetTrackSectorOffset( nVTOC_Track, 0 );
	uint8_t *pVTOC = &pDiskBytes[ nOffset ];
	Util_DOS33_SetTrackSectorUsage( pVTOC, nTrackUsed, 0x0000 );
}

//===========================================================================
void Util_DOS33_FormatFileSystem ( uint8_t *pDiskBytes, const size_t nDiskSize, const int nVTOC_Track )
{
	const int nTracks = nDiskSize / TRACK_DENIBBLIZED_SIZE;
	int nOffset;

	assert (nTracks <= TRACKS_MAX);

	// Update CATALOG next track/sector
	for( int iSector = 0xF; iSector > 1; iSector-- )
	{
		nOffset = Util_GetTrackSectorOffset( nVTOC_Track, iSector );
		pDiskBytes[ nOffset + 1 ] = nVTOC_Track;
		pDiskBytes[ nOffset + 2 ] = iSector - 1;
	}

	// Last sector in CATALOG has no link
	nOffset = Util_GetTrackSectorOffset( nVTOC_Track, 1 );
	pDiskBytes[ nOffset + 1 ] = 0;
	pDiskBytes[ nOffset + 2 ] = 0;

	// FTOC = 256 bytes
	//      - HeaderSize = 0x0C
	//                     0x00 Wasted byte
	//                     0x01 Track Next FTOC
	//                     0x02 Sector Next FTOC
	//                     0x03 Wasted byte
	//                     0x04 Wasted byte
	//                     0x05 Sector offset of file in this T/S
	//                     0x06 Sector offset of file in this T/S
	//                     0x07 Wasted byte
	//                     0x08 Wasted byte
	//                     0x09 Wasted byte
	//                     0x0A Wasted byte
	//                     0x0B Wasted byte
	//      / 2 bytes for next Track/Sector
	//      = 122 entries
	const uint8_t FTOC_ENTRIES = (256 - 12) / 2;

	nOffset = Util_GetTrackSectorOffset( nVTOC_Track, 0 );
	pDiskBytes[ nOffset +  0x1 ] = nVTOC_Track;            // CATALOG = T11
	pDiskBytes[ nOffset +  0x2 ] = 0xF;                    // CATALOG = S0F
	pDiskBytes[ nOffset +  0x3 ] = 0x3;                    // DOS 3.3
	pDiskBytes[ nOffset +  0x6 ] = DEFAULT_VOLUME_NUMBER; // Volume
	pDiskBytes[ nOffset + 0x27 ] = FTOC_ENTRIES;           // TrackSector pairs
	pDiskBytes[ nOffset + 0x30 ] = nVTOC_Track;            // Last Track Allocated
	pDiskBytes[ nOffset + 0x31 ] = 1;                      // Direction = +1
	pDiskBytes[ nOffset + 0x34 ] = nTracks;                // Number of Tracks based on image size
	pDiskBytes[ nOffset + 0x35 ] = 16;                     // 16 Sectors/Track
	pDiskBytes[ nOffset + 0x36 ] = 0x00;                   // 256 Bytes/Sector Lo
	pDiskBytes[ nOffset + 0x37 ] = 0x01;                   // 256 Bytes/Sector Hi

	uint8_t *pVTOC = &pDiskBytes[ nOffset ];
	for( int iTrack = 0; iTrack < nTracks; iTrack++ )
	{
		/**/ if (iTrack ==           0) Util_DOS33_SetTrackSectorUsage(pVTOC, iTrack, 0x0000); // Track T00 can NEVER be free due to stupid DOS 3.3 design (1 byte bug of `BNE` instead of `BPL`)
		else if (iTrack == nVTOC_Track) Util_DOS33_SetTrackSectorUsage(pVTOC, iTrack, 0x0000);
		else /*                      */ Util_DOS33_SetTrackSectorUsage(pVTOC, iTrack, 0xFFFF); // Tracks T01-T10, T12-T22 are free for use
	}
}

//===========================================================================
int Util_SelectDiskImage ( const HWND hwnd, const HINSTANCE hInstance, const TCHAR* pTitle, const bool bSave, char *pFilename, const char *pFilter )
{
	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hwnd;
	ofn.hInstance       = hInstance;
	ofn.lpstrFilter     = pFilter;
	ofn.lpstrFile       = pFilename;
	ofn.nMaxFile        = MAX_PATH; // sizeof(szFilename);
	ofn.lpstrInitialDir = g_sCurrentDir.c_str();
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pTitle;

	if( bSave )
		ofn.Flags |= OFN_FILEMUSTEXIST;

	int nRes = bSave ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);
	return nRes;
}

// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winui/winui/windowsuserinterface/resources/menus/usingmenus.asp
// http://www.codeproject.com/menu/MenusForBeginners.asp?df=100&forumid=67645&exp=0&select=903061

void Win32Frame::ProcessDiskPopupMenu(HWND hwnd, POINT pt, const int iDrive)
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

	static uint32_t bNewDiskCopyBASIC     = true;
	static uint32_t bNewDiskCopyBitsyBoot = true;
	static uint32_t bNewDiskCopyBitsyBye  = true;
	static uint32_t bNewDiskCopyProDOS    = true;

	const TCHAR REG_KEY_DISK_PREFRENCES[] = TEXT("Preferences"); // NOTE: Keep in sync with REG_KEY_DISK_PREFRENCES and UtilPopup_Toggle

	RegLoadValue( REG_KEY_DISK_PREFRENCES, REGVALUE_PREF_NEW_DISK_COPY_BASIC     , TRUE, &bNewDiskCopyBASIC     );
	RegLoadValue( REG_KEY_DISK_PREFRENCES, REGVALUE_PREF_NEW_DISK_COPY_BITSY_BOOT, TRUE, &bNewDiskCopyBitsyBoot );
	RegLoadValue( REG_KEY_DISK_PREFRENCES, REGVALUE_PREF_NEW_DISK_COPY_BITSY_BYE , TRUE, &bNewDiskCopyBitsyBye  );
	RegLoadValue( REG_KEY_DISK_PREFRENCES, REGVALUE_PREF_NEW_DISK_COPY_PRODOS_SYS, TRUE, &bNewDiskCopyProDOS    );

	class UtilPopup_Toggle
	{
		public:
			UtilPopup_Toggle( HMENU hMenu, int iCommand, const char *pKey, uint32_t *pVal )
			{
				*pVal = ! *pVal;
				int bChecked = *pVal ? MF_CHECKED : MF_UNCHECKED;
				CheckMenuItem(hMenu,iCommand, bChecked);

				RegSaveValue(
					TEXT("Preferences"), // NOTE: Keep in sync with REG_KEY_DISK_PREFRENCES and UtilPopup_Toggle
					pKey,
					TRUE,
					*pVal
				);
			}
	};

	class UtilPopup_CheckMenu
	{
		public:
			UtilPopup_CheckMenu( HMENU hMenu, int iMenuItem, uint32_t bIsChecked )
			{
				if (bIsChecked)
				{
					CheckMenuItem(hMenu, iMenuItem, MF_CHECKED);
				}
			}
	};

	std::string filename1= "\"";
	filename1.append( disk2Card.GetFullName(iDrive) );
	filename1.append("\"");
	std::string sFileNameEmpty = "\"";
	sFileNameEmpty.append("\"");

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

	bool bIsProtected  = disk2Card.GetProtect(iDrive);
	bool bIsDriveEmpty = disk2Card.IsDriveEmpty(iDrive);
	HINSTANCE hInstance = GetFrame().g_hInstance;

	// Check menu depending on current floppy protection
	{
		int iMenuItem = ID_DISKMENU_WRITEPROTECTION_OFF;
		if (bIsProtected)
			iMenuItem = ID_DISKMENU_WRITEPROTECTION_ON;

		CheckMenuItem(hmenu, iMenuItem, MF_CHECKED);
	}

	if (bIsDriveEmpty)
	{
		EnableMenuItem(hmenu, ID_DISKMENU_EJECT, MF_GRAYED);
	}

	if (bIsProtected)
	{
		// If image-file is read-only (or a gzip) then disable these menu items
		EnableMenuItem(hmenu, ID_DISKMENU_WRITEPROTECTION_ON, MF_GRAYED);
		EnableMenuItem(hmenu, ID_DISKMENU_WRITEPROTECTION_OFF, MF_GRAYED);

	}

	// New Disk options for ProDOS file copy on format
	{
		UtilPopup_CheckMenu( hmenu, ID_DISKMENU_NEW_DISK_COPY_PRODOS    , bNewDiskCopyProDOS    );
		UtilPopup_CheckMenu( hmenu, ID_DISKMENU_NEW_DISK_COPY_BITSY_BOOT, bNewDiskCopyBitsyBoot );
		UtilPopup_CheckMenu( hmenu, ID_DISKMENU_NEW_DISK_COPY_BITSY_BYE , bNewDiskCopyBitsyBye  );
		UtilPopup_CheckMenu( hmenu, ID_DISKMENU_NEW_DISK_COPY_BASIC     , bNewDiskCopyBASIC     );
	};

	// Disk images QoL always enabled since they prompt which disk image to create/modify.
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_PRODOS_140K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_PRODOS_160K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_PRODOS_800K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_PRODOS_32MB_DISK, MF_ENABLED);

	EnableMenuItem(hmenu, ID_DISKMENU_NEW_DOS33_140K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_DOS33_160K_DISK, MF_ENABLED);

	ModifyMenu    (hmenu, ID_DISKMENU_ADVANCED_SEPARATOR , MF_BYCOMMAND | MF_SEPARATOR , ID_DISKMENU_ADVANCED_SEPARATOR, 0 );
	EnableMenuItem(hmenu, ID_DISKMENU_SELECT_BOOT_SECTOR , MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_BLANK_140K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_BLANK_160K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_BLANK_800K_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_NEW_BLANK_32MB_DISK, MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_FORMAT_PRODOS_DISK , MF_ENABLED);
	EnableMenuItem(hmenu, ID_DISKMENU_FORMAT_DOS33_DISK  , MF_ENABLED);

#if _DEBUG
	// Never hide menu options in debug build
#else
	// If SHIFT is pressed enable the advanced disk pop-up menus
	// otherwise remove them (since there is no Win32 comamnd to hide menu items.)
	//
	// KeybGetCtrlStatus(); // Can't use since Ctrl-F3, Ctrl-F4 is shortcut
	// KeybGetAltStatus();  // Can't use
	bool bAdvanced = KeybGetShiftStatus(); // #1364
	if (!bAdvanced)
	{
		RemoveMenu(hmenu, ID_DISKMENU_ADVANCED_SEPARATOR , MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_SELECT_BOOT_SECTOR , MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_NEW_BLANK_140K_DISK, MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_NEW_BLANK_160K_DISK, MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_NEW_BLANK_800K_DISK, MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_NEW_BLANK_32MB_DISK, MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_FORMAT_PRODOS_DISK , MF_BYCOMMAND);
		RemoveMenu(hmenu, ID_DISKMENU_FORMAT_DOS33_DISK  , MF_BYCOMMAND);
	}
#endif

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
			int MB_Result = FrameMessageBox("No disk image loaded. Do you want to run CiderPress anyway?" ,"No disk image.", MB_ICONINFORMATION|MB_YESNO);
			if (MB_Result == IDYES)
			{
				if (FileExists (PathToCiderPress ))
				{
					HINSTANCE nResult  = ShellExecute(NULL, "open", PathToCiderPress, "" , NULL, SW_SHOWNORMAL);
				}
				else
				{
					FrameMessageBox(szCiderpressNotFoundText, szCiderpressNotFoundCaption, MB_ICONINFORMATION|MB_OK);
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
				FrameMessageBox(szCiderpressNotFoundText, szCiderpressNotFoundCaption, MB_ICONINFORMATION|MB_OK);
			}
		}
	}
	else
	if((iCommand == ID_DISKMENU_NEW_PRODOS_140K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_PRODOS_160K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_PRODOS_800K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_PRODOS_32MB_DISK)
	|| (iCommand == ID_DISKMENU_NEW_DOS33_140K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_DOS33_160K_DISK))
	{
		const bool   bIsDOS33     = ((iCommand == ID_DISKMENU_NEW_DOS33_140K_DISK ) || (iCommand == ID_DISKMENU_NEW_DOS33_160K_DISK));
		const bool   bIsFloppy    =!((iCommand == ID_DISKMENU_NEW_PRODOS_800K_DISK) || (iCommand == ID_DISKMENU_NEW_PRODOS_32MB_DISK));
		const bool   bIsFloppy525 = ((iCommand == ID_DISKMENU_NEW_PRODOS_140K_DISK) || (iCommand == ID_DISKMENU_NEW_PRODOS_160K_DISK) || bIsDOS33);
		const bool   bIs40Track   = ((iCommand == ID_DISKMENU_NEW_PRODOS_160K_DISK) || (iCommand == ID_DISKMENU_NEW_DOS33_160K_DISK));
		const bool   bIsUnidisk35 =  (iCommand == ID_DISKMENU_NEW_PRODOS_800K_DISK);
		const bool   bIsHardDisk  =  (iCommand == ID_DISKMENU_NEW_PRODOS_32MB_DISK);
		const size_t nDiskSize    = bIsHardDisk
									? HARDDISK_32M_SIZE
									: bIsUnidisk35
									  ? UNIDISK35_800K_SIZE
									  : bIs40Track
									    ? TRACK_DENIBBLIZED_SIZE * TRACKS_MAX
									    : TRACK_DENIBBLIZED_SIZE * TRACKS_STANDARD
									    ;

		const TCHAR* pszTitle = TEXT("Select new blank disk image");

		time_t timestamp = time( NULL );
		tm datetime = *localtime(&timestamp);

		const size_t MAX_MONTH_LEN = 32;
		int   year               = datetime.tm_year + 1900;
		char  mon[MAX_MONTH_LEN] = {0};
		int   day                = datetime.tm_mday;
		int   hour               = datetime.tm_hour;
		int   min                = datetime.tm_min;
		int   sec                = datetime.tm_sec;
		strftime( mon, MAX_MONTH_LEN-1, "%b", &datetime );

		std::string sExtension = bIsHardDisk
			? ".hdv"
			: bIsDOS33
			  ? ".do"
			  : ".po"
			    ;

		std::string sFileName( StrFormat(
			  "blank_%s_%04d_%3s_%02d_%02dh_%02dm_%02ds%s"
			, bIsFloppy ? "floppy" : "hard"
			, year, mon, day, hour, min, sec
			, sExtension.c_str()
		));

		std::string pathname = g_sCurrentDir;
		pathname.append( sFileName );

		const TCHAR *pTitle  = TEXT("New Disk Image");
		const TCHAR *pSaveFilter = TEXT("All Files\0*.*\0");
		char sPathName[ MAX_PATH ];
		strncpy( sPathName, sFileName.c_str(), MAX_PATH-1 );
		int nRes = Util_SelectDiskImage( hwnd, hInstance, pTitle, true, sPathName, pSaveFilter );
		if (nRes)
		{
			pathname = sPathName;
			if (FileExists(pathname))
			{
				nRes = FrameMessageBox(
					  "WARNING: Disk image already exists!\n"
					  "\n"
					  "(ALL DATA WILL BE LOST!)\n"
					  "\n"
					  "Overwrite ?"
					, pTitle
					, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2 );
				if (nRes == IDNO) nRes = 0;
				else              nRes = IDOK;
			}

			if (nRes)
			{
				FILE *hFile = fopen( pathname.c_str(), "wb");
				if (hFile)
				{
					uint8_t *pDiskBytes = new uint8_t[ nDiskSize ];
					memset( pDiskBytes, 0, nDiskSize );

					if (bIsDOS33)
					{
						// File System
						int VTOC_TRACK = 0x11; // TODO: Allow user to over-ride via command-line argument? --vtoc 17
						Util_DOS33_FormatFileSystem( pDiskBytes, nDiskSize, VTOC_TRACK );
						
						// Boot Sector + OS
						const size_t nDOS33Size = 3 * 16 * 256; // First 3 tracks * 16 sectors/track * 256 bytes/sector
						const BYTE  *pDOS33Data = GetResource(IDR_OS_DOS33, "FIRMWARE", nDOS33Size);
						if (pDOS33Data)
						{
							memcpy( pDiskBytes, pDOS33Data, nDOS33Size );

							// DOS 3.3 resides on Track 0, 1, 2
							// Track 0 was already reserved when the file system was formatted.
							Util_DOS33_SetTrackUsed( pDiskBytes, VTOC_TRACK, 1 );
							Util_DOS33_SetTrackUsed( pDiskBytes, VTOC_TRACK, 2 );
						}
						else
						{
							FrameMessageBox( "WARNING: Could't find built-in DOS 3.3 Operating System!\n\nDisk will have no boot sector.", pTitle, MB_OK|MB_ICONINFORMATION);
						}
					}
					else // ProDOS
					{
							const size_t   nBootSectorsSize = 2 * 512;
							const uint8_t *pBootSectorsData = (uint8_t*) GetResource(IDR_BOOT_SECTOR_PRODOS243, "FIRMWARE", nBootSectorsSize);
							assert(pBootSectorsData);

							SectorOrder_e eSectorOrder = INTERLEAVE_PRODOS_ORDER;
							const char *pVolumeName = "BLANK";
							Util_ProDOS_ForwardSectorInterleave( pDiskBytes, nDiskSize, eSectorOrder );
							Util_ProDOS_FormatFileSystem       ( pDiskBytes, nDiskSize, pVolumeName  );
							memcpy(pDiskBytes, pBootSectorsData, nBootSectorsSize);
							if (bNewDiskCopyBitsyBoot) Util_ProDOS_CopyBitsyBoot( pDiskBytes, nDiskSize, pVolumeName, this );
							if (bNewDiskCopyBitsyBye)  Util_ProDOS_CopyBitsyBye ( pDiskBytes, nDiskSize, pVolumeName, this );
							if (bNewDiskCopyBASIC)     Util_ProDOS_CopyBASIC    ( pDiskBytes, nDiskSize, pVolumeName, this );
							if (bNewDiskCopyProDOS)    Util_ProDOS_CopyDOS      ( pDiskBytes, nDiskSize, pVolumeName, this );
							Util_ProDOS_ReverseSectorInterleave( pDiskBytes, nDiskSize, eSectorOrder );
					}

					fwrite( pDiskBytes, 1, nDiskSize, hFile );
					fclose( hFile );
				}
				else
				{
					MessageBox( hwnd, TEXT("ERROR: Couldn't open new disk image."), pTitle, MB_OK );
				}
			}
		}
	}
	else // --- New Disk Options ---
	if (iCommand == ID_DISKMENU_NEW_DISK_COPY_PRODOS)
	{
		UtilPopup_Toggle toggle( hmenu, iCommand, REGVALUE_PREF_NEW_DISK_COPY_PRODOS_SYS, &bNewDiskCopyProDOS );
	}
	else
	if (iCommand == ID_DISKMENU_NEW_DISK_COPY_BITSY_BOOT)
	{
		UtilPopup_Toggle toggle( hmenu, iCommand, REGVALUE_PREF_NEW_DISK_COPY_BITSY_BOOT, &bNewDiskCopyBitsyBoot );
	}
	else
	if (iCommand == ID_DISKMENU_NEW_DISK_COPY_BITSY_BYE)
	{
		UtilPopup_Toggle toggle( hmenu, iCommand, REGVALUE_PREF_NEW_DISK_COPY_BITSY_BYE, &bNewDiskCopyBitsyBye );
	}
	else
	if (iCommand == ID_DISKMENU_NEW_DISK_COPY_BASIC)
	{
		UtilPopup_Toggle toggle( hmenu, iCommand, REGVALUE_PREF_NEW_DISK_COPY_BASIC, &bNewDiskCopyBASIC );
	}
	else // --- Advanced ---
	if (iCommand == ID_DISKMENU_SELECT_BOOT_SECTOR)
	{
		// Default to directory of g_cmdLine.sBootSectorFileName;
		char sDisplayFileName[ 256+1 ] = {0};
		char sFilename[ MAX_PATH ] = {0};
		const size_t nDisplayFileNameMax = sizeof(sDisplayFileName);
		const size_t nFilenameMax        = sizeof(sFilename);
		const TCHAR *pTitle  = TEXT("Select boot sector file");
		const TCHAR *pExistFilter = // No *.nib;*.woz;*.gz;*.zip
			TEXT("Floppy Disk Images (*.bin,*.dsk,*.do;*.po)\0"
			                         "*.bin;*.dsk;*.do;*.po\0")
			TEXT("Hard Disk Images (*.hdv;)\0"
			                       "*.hdv\0")
			TEXT("All Files\0*.*\0");

		strncpy( sDisplayFileName, g_cmdLine.sBootSectorFileName.c_str(), nDisplayFileNameMax - 2);
		strncpy( sFilename       , g_cmdLine.sBootSectorFileName.c_str(), nFilenameMax        - 2);

		sDisplayFileName[nDisplayFileNameMax-1] = 0;
		sFilename       [nFilenameMax       -1] = 0;

		if (!g_cmdLine.nBootSectorFileSize)
		{
			strcpy( sDisplayFileName, "(Built-in AppleWin boot sector)" );
		}
		else
		if (g_cmdLine.sBootSectorFileName.length() > (nDisplayFileNameMax-1))
		{
			// Convert long filename to "smart" ellipsis
			// +126 = First 126 chars filename
			// +  4 = "...."
			// +126 = Last 126 chars of filename
			//= 256 chars
			const char *pHead = g_cmdLine.sBootSectorFileName.c_str();
			const char *pTail = pHead + g_cmdLine.sBootSectorFileName.length() - 126;
			sDisplayFileName[0] = 0;
			strncpy( sDisplayFileName +   0, pHead , 126 );
			strncpy( sDisplayFileName + 126, "....",   4 );
			strncpy( sDisplayFileName + 130, pTail , 126 );
		}

		std::string sMessage( StrFormat(
			  "Current boot sector file is: \n"
			  "\n"
			  "%s\n"
			  "\n"
			  "Use a custom boot sector file?\n"
			, sDisplayFileName
		));
		
		// Show dialog with current boot sector disk image
		// Ask user if they wish to replace it
		//
		//   [Yes]    = Use custom boot sector
		//   [No]     = Use AppleWin boot sector
		//   [Cancel] = Quit
		int nRes = FrameMessageBox(sMessage.c_str(), pTitle, MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON2);
		if (nRes == IDNO)
		{
			g_cmdLine.nBootSectorFileSize = 0;
			g_cmdLine.sBootSectorFileName = "";
		}
		else
		if (nRes == IDYES)
		{
			int nRes = Util_SelectDiskImage( hwnd, hInstance, pTitle, false, sFilename, pExistFilter );
			if (nRes)
			{
				char  sPath[ MAX_PATH];
				DWORD res = GetFullPathName(sFilename, MAX_PATH, sPath, NULL);

				FILE *pFile = fopen( sPath, "rb");
				if (pFile)
				{
					fseek( pFile, 0, SEEK_END );
					size_t nSize = ftell( pFile );
					fseek( pFile, 0, SEEK_SET );
					fclose( pFile );

					if (nSize > 0)
					{
						g_cmdLine.sBootSectorFileName = sPath;
						g_cmdLine.nBootSectorFileSize = nSize;
					}
					else
					{
						g_cmdLine.nBootSectorFileSize = 0;
						g_cmdLine.sBootSectorFileName = "";
						FrameMessageBox("ERROR: Couldn't read custom boot sector file.\n\nDefaulting to built-in AppleWin Boot Sector.", pTitle, MB_ICONERROR | MB_OK);
					}
				}
				else
				{
					g_cmdLine.nBootSectorFileSize = 0;
					g_cmdLine.sBootSectorFileName = "";
					FrameMessageBox( "ERROR: Couldn't open custom boot sector file.\n\nDefaulting to built-in AppleWin Boot Sector.", pTitle, MB_ICONERROR | MB_OK);
				}
			}
		}
	}
	else // --- Advanced ---
	if((iCommand == ID_DISKMENU_NEW_BLANK_140K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_BLANK_160K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_BLANK_800K_DISK)
	|| (iCommand == ID_DISKMENU_NEW_BLANK_32MB_DISK))
	{
		const bool   bIsFloppy    =  (iCommand != ID_DISKMENU_NEW_BLANK_32MB_DISK);
		const bool   bIsFloppy525 = ((iCommand == ID_DISKMENU_NEW_BLANK_140K_DISK) || (iCommand == ID_DISKMENU_NEW_BLANK_160K_DISK));
		const bool   bIs40Track   =  (iCommand == ID_DISKMENU_NEW_BLANK_160K_DISK);
		const bool   bIsUnidisk35 =  (iCommand == ID_DISKMENU_NEW_BLANK_800K_DISK);
		const bool   bIsHardDisk  =  (iCommand == ID_DISKMENU_NEW_BLANK_32MB_DISK);
		const size_t nDiskSize    = bIsHardDisk
									? HARDDISK_32M_SIZE
									: bIsUnidisk35
									  ? UNIDISK35_800K_SIZE
									  : bIs40Track
									    ? TRACK_DENIBBLIZED_SIZE * TRACKS_MAX
									    : TRACK_DENIBBLIZED_SIZE * TRACKS_STANDARD
									    ;

		const TCHAR* pszTitle = TEXT("Select new blank disk image");

		time_t timestamp = time( NULL );
		tm datetime = *localtime(&timestamp);

		const size_t MAX_MONTH_LEN = 32;
		int   year               = datetime.tm_year + 1900;
		char  mon[MAX_MONTH_LEN] = {0};
		int   day                = datetime.tm_mday;
		int   hour               = datetime.tm_hour;
		int   min                = datetime.tm_min;
		int   sec                = datetime.tm_sec;
		strftime( mon, MAX_MONTH_LEN-1, "%b", &datetime );
		std::string sFileName( StrFormat(
			  "blank_%s_%04d_%3s_%02d_%02dh_%02dm_%02ds.%s"
			, bIsFloppy ? "floppy" : "hard"
			, year, mon, day, hour, min, sec
			, bIsFloppy ? "dsk" : "hdv"
		) );

		std::string pathname = g_sCurrentDir;
		pathname.append( sFileName );

		const TCHAR *pTitle  = TEXT("New Disk Image");
		const TCHAR *pSaveFilter = TEXT("All Files\0*.*\0");
		char sPathName[ MAX_PATH ];
		strncpy( sPathName, sFileName.c_str(), MAX_PATH-1 );
		int nRes = Util_SelectDiskImage( hwnd, hInstance, pTitle, true, sPathName, pSaveFilter );
		if (nRes)
		{
			pathname = sPathName;
			if (FileExists(pathname))
			{
				nRes = FrameMessageBox(
					  "WARNING: Disk image already exists!\n"
					  "\n"
					  "(ALL DATA WILL BE LOST!)\n"
					  "\n"
					  "Overwrite ?"
					, pTitle
					, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2 );
				if (nRes == IDNO) nRes = 0;
				else              nRes = IDOK;
			}

			if (nRes)
			{
				FILE *hFile = fopen( pathname.c_str(), "wb");
				if (hFile)
				{
					size_t  nDiskBuffer = nDiskSize;
					char   *pDiskBuffer = new char[ nDiskSize ];
					memset( pDiskBuffer, 0, nDiskSize );

					// See: firmware/BootSector/bootsector.a
					uint8_t  aAppleWinBootSector[256];
					bool     bUseAppleWinBootSector = !g_cmdLine.nBootSectorFileSize; // Can have custom boot loader via command line: -bootsector <file>
					size_t   nBootSectorSize = bUseAppleWinBootSector ? sizeof(aAppleWinBootSector) :              g_cmdLine.nBootSectorFileSize  ;
					uint8_t *pBootSector     = bUseAppleWinBootSector ?        aAppleWinBootSector  : new uint8_t[ g_cmdLine.nBootSectorFileSize ];
					if (bUseAppleWinBootSector)
					{
						BYTE *pAppleWinBootSector = GetResource(IDR_BOOT_SECTOR, "FIRMWARE", sizeof(aAppleWinBootSector));
						if (pAppleWinBootSector)
						{
							memcpy(aAppleWinBootSector, pAppleWinBootSector, sizeof(aAppleWinBootSector));

							if (bIsHardDisk)
							{
								static_assert( sizeof(aAppleWinBootSector) == 256 );
								// Modify boot message depending on type of disk
								// Floppy: THIS IS AN EMPTY DATA DISK.
								// Hard  : THIS IS AN EMPTY HARD DISK.
								//                          ^^^^
								size_t nOffsetData = aAppleWinBootSector[ 0xFF ]; // MAGIC NUMBER: Last byte has offset of text message. See: firmware/BootSector/bootsector.a
								if (nOffsetData > 0)
								{
									aAppleWinBootSector[ nOffsetData+0 ] = 0x80 | 'H';
									aAppleWinBootSector[ nOffsetData+1 ] = 0x80 | 'A';
									aAppleWinBootSector[ nOffsetData+2 ] = 0x80 | 'R';
									aAppleWinBootSector[ nOffsetData+3 ] = 0x80 | 'D';
								}
							}
						}
						else
						{
							FrameMessageBox( "WARNING: Could't find built-in AppleWin boot sector!\n\nDisk will have no boot sector.", pTitle, MB_OK|MB_ICONINFORMATION);
							nBootSectorSize = 0;
						}
					}
					else
					{
						FILE *pFile = fopen( g_cmdLine.sBootSectorFileName.c_str(), "rb" );
						if (pFile)
						{
							size_t nSize = MIN(g_cmdLine.nBootSectorFileSize, nDiskSize );
							size_t nRead = fread( pBootSector, 1, nSize, pFile );
							fclose( pFile );

							if (g_cmdLine.nBootSectorFileSize > nDiskSize)
							{
								std::string sMessage( StrFormat(
									  "WARNING: Custom boot sector (%zu) is larger then the disk image (%zu)!\n"
									  "\n"
									  "Restricting boot sector to disk image size."
									, g_cmdLine.nBootSectorFileSize
									, nDiskSize
								));
								FrameMessageBox( sMessage.c_str(), pTitle, MB_OK | MB_ICONINFORMATION);
							}
						}
						else
						{
							FrameMessageBox( TEXT("WARNING: Couldn't open custom boot sector file.\n\nNo boot sector written."), pTitle, MB_OK | MB_ICONERROR );
							nBootSectorSize = 0;
						}
					}

					memcpy( pDiskBuffer, pBootSector, nBootSectorSize );
					fwrite( pDiskBuffer, 1, nDiskSize, hFile );
					fclose( hFile );

					if (!bUseAppleWinBootSector)
					{
						delete [] pBootSector;
					}
				}
				else
				{
					MessageBox( hwnd, TEXT("ERROR: Couldn't open new disk image."), pTitle, MB_OK );
				}
			}
		}
	}
	else // --- Advanced ---
	if (iCommand == ID_DISKMENU_FORMAT_PRODOS_DISK)
	{
		char szFilename[ MAX_PATH ] = {0};
		const TCHAR *pTitle  = TEXT("Select ProDOS disk image to format");
		const TCHAR *pLoadFilter = // No *.nib;*.woz;*.gz;*.zip
			TEXT("Floppy Disk Images (*.bin,*.dsk,*.po)\0"
			                         "*.bin;*.dsk;*.po\0")
			TEXT("Hard Disk Images (*.hdv;)\0"
			                       "*.hdv\0")
			TEXT("All Files\0*.*\0");

		int nRes = FrameMessageBox(
			"Are you sure you want to FORMAT an existing disk as a ProDOS data disk?\n"
			"\n"
			"(ALL DATA WILL BE LOST!)\n"
			, "Format"
			, MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2
		);
		if (nRes == IDYES)
		{
			nRes = Util_SelectDiskImage( hwnd, hInstance, pTitle, true, szFilename, pLoadFilter );
			if (nRes)
			{
				std::string pathname = szFilename;
				if (FileExists( pathname ))
				{
					FILE *hFile = fopen( pathname.c_str(), "r+b" );
					if (hFile)
					{
						fseek( hFile, 0, SEEK_END );
						size_t nDiskSize = ftell( hFile );
						fseek( hFile, 0, SEEK_SET );

						size_t nMaxDiskSize = HARDDISK_32M_SIZE;

						if (nDiskSize > nMaxDiskSize)
						{
							std::string sMessage(StrFormat(
								  "ERROR: Disk Image Size (%zu bytes) > maximum ProDOS volume size (%zu bytes)"
								, nDiskSize
								, nMaxDiskSize
							));
							FrameMessageBox( sMessage.c_str(), "Format", MB_ICONWARNING | MB_OK);
						}
						else
						{
							uint8_t *pDiskBytes = new uint8_t[ nDiskSize ];
							size_t nReadSize = fread( pDiskBytes, 1, nDiskSize, hFile );
							assert( nReadSize == nDiskSize );

							// We can have DOS or ProDOS sector interleaving
							//     Extension  Interleave
							//     .bin       Unknown, assume DOS
							//     .dsk       DOS
							//     .po        ProDOS
							//     .hdv       ProDOS
							SectorOrder_e eSectorOrder = Util_Disk_CalculateSectorOrder( pathname );
							if (eSectorOrder == INTERLEAVE_AUTO_DETECT)
							{
								int res = FrameMessageBox(
									"Unable to auto-detect the disk image sector order!\n"
									"\n"
									"Is this image using a ProDOS sector order?\n"
									"(No will use DOS 3.3 sector order)"
									, "Format", MB_ICONWARNING|MB_YESNO);
								eSectorOrder = (res == IDYES)
								             ? INTERLEAVE_PRODOS_ORDER
								             : INTERLEAVE_DOS33_ORDER
							                 ;
							}
							assert (eSectorOrder !=  INTERLEAVE_AUTO_DETECT);

							const char *pVolumeName = "BLANK";
							Util_ProDOS_ForwardSectorInterleave( pDiskBytes, nDiskSize, eSectorOrder );
							Util_ProDOS_FormatFileSystem       ( pDiskBytes, nDiskSize, pVolumeName  );
							Util_ProDOS_ReverseSectorInterleave( pDiskBytes, nDiskSize, eSectorOrder );

							fseek( hFile, 0, SEEK_SET );
							size_t nWroteSize = fwrite( pDiskBytes, 1, nReadSize, hFile );
							if (nWroteSize != nDiskSize)
							{
								FrameMessageBox( "ERROR: Unable to write ProDOS File System", "Format", MB_ICONWARNING | MB_OK);
							}

							delete [] pDiskBytes;
						}
						fclose( hFile );
					}
				}
			}
		}
	}
	else
	if (iCommand == ID_DISKMENU_FORMAT_DOS33_DISK)
	{
		char szFilename[ MAX_PATH ] = {0};
		const TCHAR *pTitle  = TEXT("Select DOS 3.3 disk image to format");
		const TCHAR *pLoadFilter =
			TEXT("Floppy Disk Images (*.bin,*.dsk,*.do)\0"
			                         "*.bin;*.dsk;*.do\0")
			TEXT("All Files\0*.*\0");

		int nRes = FrameMessageBox(
			"Are you sure you want to FORMAT an existing disk as a DOS 3.3 data disk?\n"
			"\n"
			"(ALL DATA WILL BE LOST!)\n"
			, "Format", MB_ICONWARNING|MB_YESNO);
		if (nRes == IDYES)
		{
			nRes = Util_SelectDiskImage( hwnd, hInstance, pTitle, false, szFilename, pLoadFilter );
			if (nRes)
			{
				std::string pathname = szFilename;
				if (FileExists( pathname ))
				{
					FILE *hFile = fopen( pathname.c_str(), "r+b" );
					if (hFile)
					{
						fseek( hFile, 0, SEEK_END );
						size_t nDiskSize = ftell( hFile );
						fseek( hFile, 0, SEEK_SET );

						// Verify floppy size is <= 160KB (max 40 tracks) since that is the largest supported by DOS 3.3
						// TODO: Maybe use CImageBase::IsValidImageSize() ?
						size_t nMinDiskSize = 34         *  TRACK_DENIBBLIZED_SIZE;
						size_t nMaxDiskSize = TRACKS_MAX * TRACK_DENIBBLIZED_SIZE;

						std::string sMessage;
						if (nDiskSize < nMinDiskSize)
						{
							sMessage = StrFormat( "ERROR: Disk image size (%zu bytes) < minimum DOS 3.3 image size (%zu bytes)", nDiskSize, nMinDiskSize );
							FrameMessageBox( sMessage.c_str(), "Format", MB_ICONERROR | MB_OK);
						}
						else
						if (nDiskSize > nMaxDiskSize)
						{
							sMessage = StrFormat( "ERROR: Disk image size (%zu bytes) > maximum DOS 3.3 image size (%zu bytes)", nDiskSize, nMaxDiskSize );
							FrameMessageBox( sMessage.c_str(), "Format", MB_ICONERROR | MB_OK);
						}
						else
						{
							uint8_t *pDiskBytes = new uint8_t[ nDiskSize ];
							size_t nReadSize = fread( pDiskBytes, 1, nDiskSize, hFile );
							assert( nReadSize == nDiskSize );

							int VTOC_TRACK = 0x11; // TODO: Allow user to over-ride via command-line argument? --vtoc 17
							Util_DOS33_FormatFileSystem( pDiskBytes, nDiskSize, VTOC_TRACK );

							fseek( hFile, 0, SEEK_SET );
							size_t nWroteSize = fwrite( pDiskBytes, 1, nReadSize, hFile );
							if (nWroteSize != nDiskSize)
							{
								FrameMessageBox( "ERROR: Unable to write DOS 3.3 file system", "Format", MB_ICONWARNING | MB_OK);
							}
							delete [] pDiskBytes;
						}
						fclose( hFile );
					}
					else
					{
						FrameMessageBox( "ERROR: Unable to open disk image for writing DOS 3.3 file system", "Format", MB_ICONWARNING | MB_OK);
					}
				}
			}
		}
	}

	// Destroy the menu.
	BOOL bRes = DestroyMenu(hmenu);
	_ASSERT(bRes);
}


//===========================================================================
void Win32Frame::RelayEvent (UINT message, WPARAM wparam, LPARAM lparam) {
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

int Win32Frame::GetFullScreenOffsetX(void)
{
	return g_win_fullscreen_offsetx;
}

int Win32Frame::GetFullScreenOffsetY(void)
{
	return g_win_fullscreen_offsety;
}

void Win32Frame::SetFullScreenMode(void)
{
#ifdef NO_DIRECT_X

	return;

#else // NO_DIRECT_X

	if (m_bestWidthForFullScreen && m_bestHeightForFullScreen)
	{
		DEVMODE devMode;
		memset(&devMode, 0, sizeof(devMode));
		devMode.dmSize = sizeof(devMode);
		devMode.dmPelsWidth = m_bestWidthForFullScreen;
		devMode.dmPelsHeight = m_bestHeightForFullScreen;
		devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

		uint32_t dwFlags = 0;
		LONG res = ChangeDisplaySettings(&devMode, dwFlags);
		m_changedDisplaySettings = true;
	}

	//

	MONITORINFO monitor_info;
	FULLSCREEN_SCALE_TYPE width, height, scalex, scaley;
	int top, left;

	buttonover = -1;

	g_main_window_saved_style = GetWindowLong(g_hFrameWindow, GWL_STYLE);
	g_main_window_saved_exstyle = GetWindowLong(g_hFrameWindow, GWL_EXSTYLE);
	GetWindowRect(g_hFrameWindow, &g_main_window_saved_rect);
	SetWindowLong(g_hFrameWindow, GWL_STYLE  , g_main_window_saved_style   & ~(WS_CAPTION | WS_THICKFRAME));
	SetWindowLong(g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
	
	monitor_info.cbSize = sizeof(monitor_info);
	GetMonitorInfo(MonitorFromWindow(g_hFrameWindow, MONITOR_DEFAULTTONEAREST), &monitor_info);

	left = monitor_info.rcMonitor.left;
	top  = monitor_info.rcMonitor.top;

	width  = (FULLSCREEN_SCALE_TYPE)(monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left);
	height = (FULLSCREEN_SCALE_TYPE)(monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top );

	scalex = width  / GetVideo().GetFrameBufferBorderlessWidth();
	scaley = height / GetVideo().GetFrameBufferBorderlessHeight();

	// Retain aspect ratio if user hasn't changed full-screen resolution (GH#1121)
	if (!GetFullScreenResolutionChangedByUser())
	{
		const int minimumScale = (scalex <= scaley) ? scalex : scaley;
		scalex = scaley = minimumScale;
	}

	// NB. Separate x,y scaling is OK in full-screen mode
	// . eg. SHR 640x400 (scalex=2, scaley=3) => 1280x1200, which roughly gives a 4:3 aspect ratio for a resolution of 1600x1200
	g_win_fullscreen_offsetx = ((int)width - (int)(scalex * GetVideo().GetFrameBufferBorderlessWidth())) / 2;
	g_win_fullscreen_offsety = ((int)height - (int)(scaley * GetVideo().GetFrameBufferBorderlessHeight())) / 2;
	SetWindowPos(g_hFrameWindow, NULL, left, top, (int)width, (int)height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	g_bIsFullScreen = true;

	SetFullScreenViewportScale(scalex, scaley);

	buttonx    = GetFullScreenOffsetX() + g_nViewportCX + VIEWPORTX*2;
	buttony    = GetFullScreenOffsetY();
	viewportx  = VIEWPORTX;		// TC-TODO: Should be zero too? (Since there's no 3D border in full-screen)
	viewporty  = 0; // GH#464

	InvalidateRect(g_hFrameWindow,NULL,1);

#endif // NO_DIRECT_X
}

//===========================================================================
void Win32Frame::SetNormalMode(void)
{
	if (m_changedDisplaySettings)
	{
		// Only call ChangeDisplaySettings() if resolution has changed, otherwise there'll be a display flicker (GH#965)
		ChangeDisplaySettings(NULL, 0);	// restore default resolution
		m_changedDisplaySettings = false;
	}

	FullScreenRevealCursor();	// Do before clearing g_bIsFullScreen flag

	buttonover = -1;
	buttonx    = BUTTONX;
	buttony    = BUTTONY;
	viewportx  = VIEWPORTX;
	viewporty  = VIEWPORTY;

	g_win_fullscreen_offsetx = 0;
	g_win_fullscreen_offsety = 0;
	SetWindowLong(g_hFrameWindow, GWL_STYLE, g_main_window_saved_style);
	SetWindowLong(g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle);
	SetWindowPos(g_hFrameWindow, NULL,
		g_main_window_saved_rect.left,
		g_main_window_saved_rect.top,
		g_main_window_saved_rect.right - g_main_window_saved_rect.left,
		g_main_window_saved_rect.bottom - g_main_window_saved_rect.top,
		SWP_SHOWWINDOW);
	g_bIsFullScreen = false;
}

//===========================================================================
void Win32Frame::SetUsingCursor (BOOL bNewValue)
{
	if (bNewValue == g_bUsingCursor)
		return;

	g_bUsingCursor = bNewValue;

	if (g_bUsingCursor)
	{
		// Set TRUE when:
		// . Using mouse for joystick emulation
		// . Using mousecard and mouse is restricted to window
		SetCapture(g_hFrameWindow);
		RECT rect =	{	viewportx+2,				// left
						viewporty+2,				// top
						viewportx+g_nViewportCX-1,	// right
						viewporty+g_nViewportCY-1};	// bottom
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

int Win32Frame::GetViewportScale(void)
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

	buttonx = BUTTONX;	// NB. macro uses g_nViewportCX
	buttony = BUTTONY;

	return nNewScale;
}

void Win32Frame::SetFullScreenViewportScale(int nNewXScale, int nNewYScale)
{
	g_nViewportScale = MIN(nNewXScale, nNewYScale);	// Not needed in FS mode
	g_nViewportCX = nNewXScale * GetVideo().GetFrameBufferBorderlessWidth();
	g_nViewportCY = nNewYScale * GetVideo().GetFrameBufferBorderlessHeight();

	buttonx = BUTTONX;	// NB. macro uses g_nViewportCX
	buttony = BUTTONY;
}

void Win32Frame::SetupTooltipControls(void)
{
	TOOLINFO toolinfo;
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.uFlags = TTF_CENTERTIP;
	toolinfo.hwnd = g_hFrameWindow;
	toolinfo.hinst = g_hInstance;
	toolinfo.lpszText = LPSTR_TEXTCALLBACK;
	toolinfo.rect.left  = BUTTONX;
	toolinfo.rect.right = toolinfo.rect.left+BUTTONCX+1;

	toolinfo.uId = TTID_DRIVE1_BUTTON;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE1*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);

	toolinfo.uId = TTID_DRIVE2_BUTTON;
	toolinfo.rect.top    = BUTTONY+BTN_DRIVE2*BUTTONCY+1;
	toolinfo.rect.bottom = toolinfo.rect.top+BUTTONCY;
	SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);

	if (g_nViewportScale > 1)
	{
		if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		{
			toolinfo.uId = TTID_SLOT6_TRK_SEC_INFO;
			toolinfo.rect.top = BUTTONY + BUTTONS * BUTTONCY + 1 + yOffsetSlot6TrackInfo;
			toolinfo.rect.bottom = toolinfo.rect.top + smallfontHeight * 2;
			SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
		}

		if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
		{
			toolinfo.uId = TTID_SLOT5_TRK_SEC_INFO;
			toolinfo.rect.top = BUTTONY + BUTTONS * BUTTONCY + 1 + yOffsetSlot5TrackInfo;
			toolinfo.rect.bottom = toolinfo.rect.top + smallfontHeight * 2;
			SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
		}
	}
}

// SM_CXPADDEDBORDER is not supported on 2000 & XP, but GetSystemMetrics() returns 0 for unknown values, so this use of SM_CXPADDEDBORDER works on 2000 & XP too:
// http://msdn.microsoft.com/en-nz/library/windows/desktop/ms724385(v=vs.85).aspx
// NB. GetSystemMetrics(SM_CXPADDEDBORDER) returns 0 for Win7, when built with VS2008 (see GH#571)
void Win32Frame::GetWidthHeight(int& nWidth, int& nHeight)
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

// Window frame's border size has changed (eg. VidHD added/removed)
void Win32Frame::ResizeWindow(void)
{
	FrameResizeWindow(GetViewportScale());
}

void Win32Frame::FrameResizeWindow(int nNewScale)
{
	int nOldWidth, nOldHeight;
	GetWidthHeight(nOldWidth, nOldHeight);

	nNewScale = SetViewportScale(nNewScale);

	GetWindowRect(g_hFrameWindow, &framerect);
	int nXPos = framerect.left;
	int nYPos = framerect.top;

	// Invalidate old rect region
	{
		RECT irect;
		irect.left = irect.top = 0;
		irect.right = nOldWidth;
		irect.bottom = nOldHeight;
		InvalidateRect(g_hFrameWindow, &irect, TRUE);
	}

	// Resize the window
	int nNewWidth, nNewHeight;
	GetWidthHeight(nNewWidth, nNewHeight);

	MoveWindow(g_hFrameWindow, nXPos, nYPos, nNewWidth, nNewHeight, TRUE);
	UpdateWindow(g_hFrameWindow);

	// Remove the tooltips for the old window size
	TOOLINFO toolinfo = {0};
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.hwnd = g_hFrameWindow;

	for (UINT id = 0; id < TTID_MAX; id++)
	{
		toolinfo.uId = id;
		SendMessage(tooltipwindow, TTM_DELTOOL, 0, (LPARAM)&toolinfo);
	}

	// Setup the tooltips for the new window size
	SetupTooltipControls();
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void Win32Frame::FrameCreateWindow(void)
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
		SetViewportScale(1);
		GetWidthHeight(nWidth, nHeight);
	}

	// Restore Window X Position
	int nXPos = -1;
	{
		const int nXScreen = GetSystemMetrics(SM_CXSCREEN) - nWidth;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_X_POS), 1, (uint32_t*)&nXPos))
		{
			if ((nXPos > nXScreen) && !g_bMultiMon)
				nXPos = -1;	// Not fully visible, so default to centre position
		}

		if ((nXPos == -1) && !g_bMultiMon)
			nXPos = nXScreen / 2;
	}

	// Restore Window Y Position
	int nYPos = -1;
	{
		const int nYScreen = GetSystemMetrics(SM_CYSCREEN) - nHeight;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_Y_POS), 1, (uint32_t*)&nYPos))
		{
			if ((nYPos > nYScreen) && !g_bMultiMon)
				nYPos = -1;	// Not fully visible, so default to centre position
		}

		if ((nYPos == -1) && !g_bMultiMon)
			nYPos = nYScreen / 2;
	}

	//

	buttonx = (g_nViewportCX + VIEWPORTX*2);
	buttony = 0;

	GetAppleWindowTitle();

	// NB. g_hFrameWindow also set by WM_CREATE - NB. CreateWindow() must synchronously send WM_CREATE
	g_hFrameWindow = CreateWindow(
		TEXT("APPLE2FRAME"),
		g_pAppTitle.c_str(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_VISIBLE,
		nXPos, nYPos, nWidth, nHeight,
		HWND_DESKTOP,
		(HMENU)0,
		g_hInstance, NULL );

	InitCommonControls();
	tooltipwindow = CreateWindow(
		TOOLTIPS_CLASS,NULL,TTS_ALWAYSTIP|TTS_NOPREFIX /*Allow tabs in tooltips*/ ,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, 
		g_hFrameWindow,
		(HMENU)0,
		g_hInstance,NULL );

	SetupTooltipControls();

	_ASSERT(g_TimerIDEvent_100msec == 0);
	g_TimerIDEvent_100msec = SetTimer(g_hFrameWindow, IDEVENT_TIMER_100MSEC, 100, NULL);
	LogFileOutput("FrameCreateWindow: SetTimer(), id=0x%08X\n", g_TimerIDEvent_100msec);
}

//===========================================================================
HDC Win32Frame::FrameGetDC () {
  if (!g_hFrameDC) {
    g_hFrameDC = GetDC(g_hFrameWindow);
    SetViewportOrgEx(g_hFrameDC,viewportx,viewporty,NULL);
  }
  return g_hFrameDC;
}

//===========================================================================
void Win32Frame::FrameReleaseDC () {
  if (g_hFrameDC) {
    SetViewportOrgEx(g_hFrameDC,0,0,NULL);
    ReleaseDC(g_hFrameWindow,g_hFrameDC);
    g_hFrameDC = (HDC)0;
  }
}

//===========================================================================
void Win32Frame::FrameRefreshStatus (int drawflags) {
	DrawStatusArea((HDC)0,drawflags);
}

//===========================================================================
void Win32Frame::FrameRegisterClass () {
  WNDCLASSEX wndclass;
  memset(&wndclass, 0, sizeof(WNDCLASSEX));
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

	if (!g_hFrameWindow || g_bShowingCursor)
		return;

	int iX, iMinX, iMaxX;
	int iY, iMinY, iMaxY;
	GetCardMgr().GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

	float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
	float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));

	int iWindowX = (int)(fScaleX * (float)g_nViewportCX);
	int iWindowY = (int)(fScaleY * (float)g_nViewportCY);

	POINT Point = {viewportx+2, viewporty+2};	// top-left
	ClientToScreen(g_hFrameWindow, &Point);
	SetCursorPos(Point.x+iWindowX-VIEWPORTX, Point.y+iWindowY-VIEWPORTY);

#if defined(_DEBUG) && 0	// OutputDebugString() when cursor position changes since last time
	static int OldX = 0, OldY = 0;
	int X = Point.x + iWindowX - VIEWPORTX;
	int Y = Point.y + iWindowY - VIEWPORTY;
	if (X != OldX || Y != OldY)
	{
		LogOutput("[FrameSetCursorPosByMousePos] x,y=%d,%d (MaxX,Y=%d,%d)\n", X, Y, iMaxX, iMaxY);
		OldX = X; OldY = Y;
	}
#endif
}

// Called when:
// . UpdateMouseInAppleViewport() is called and mouse leaving/entering Apple screen area
// . NB. Not called when leaving & mouse clipped to Apple screen area
void Win32Frame::FrameSetCursorPosByMousePos(int x, int y, int dx, int dy, bool bLeavingAppleScreen)
{
	_ASSERT(GetCardMgr().IsMouseCardInstalled());
	if (!GetCardMgr().IsMouseCardInstalled())
		return;

	if (!g_hFrameWindow || (g_bShowingCursor && bLeavingAppleScreen) || (!g_bShowingCursor && !bLeavingAppleScreen))
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
		ClientToScreen(g_hFrameWindow, &Point);
		SetCursorPos(Point.x+iWindowX-VIEWPORTX, Point.y+iWindowY-VIEWPORTY);
		//LogOutput("[MOUSE_LEAVING ] x=%d, y=%d (Scale: x,y=%f,%f; iX,iY=%d,%d)\n", iWindowX, iWindowY, fScaleX, fScaleY, iX, iY);
	}
	else	// Mouse entering Apple screen area
	{
		//LogOutput("[MOUSE_ENTERING] x=%d, y=%d\n", x, y);
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

void Win32Frame::DrawCrosshairsMouse()
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

void Win32Frame::UpdateMouseInAppleViewport(int iOutOfBoundsX, int iOutOfBoundsY, int x, int y)
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
			FrameSetCursorPosByMousePos();	// Set cursor to Apple position each time
		}

		DrawCrosshairsMouse();
	}
}

void Win32Frame::GetViewportCXCY(int& nViewportCX, int& nViewportCY)
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

bool Win32Frame::GetBestDisplayResolutionForFullScreen(UINT& bestWidth, UINT& bestHeight, UINT userSpecifiedWidth/*=0*/, UINT userSpecifiedHeight/*=0*/)
{
	m_bestWidthForFullScreen = 0;
	m_bestHeightForFullScreen = 0;

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

		// Pick user-specific width if it exists
		// Else pick least width (such that it's wide enough to scale)
		UINT width = (UINT)-1;
		for (VEC_PAIR::iterator it = vecDisplayResolutions.begin(); it!= vecDisplayResolutions.end(); ++it)
		{
			if (it->first == userSpecifiedWidth)
			{
				width = userSpecifiedWidth;
				break;
			}

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

		m_bestWidthForFullScreen = bestWidth;
		m_bestHeightForFullScreen = bestHeight;
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

	m_bestWidthForFullScreen = bestWidth;
	m_bestHeightForFullScreen = bestHeight;
	return true;
}
