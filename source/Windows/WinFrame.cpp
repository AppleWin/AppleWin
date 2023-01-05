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
#include "Pravets.h"
#include "Registry.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "SoundCore.h"
#include "Uthernet1.h"
#include "Uthernet2.h"
#include "Speaker.h"
#include "Utilities.h"
#include "CardManager.h"
#include "../resource/resource.h"
#include "Configuration/PropertySheet.h"
#include "Debugger/Debug.h"
#if _MSC_VER < 1900	// VS2013 or before (cl.exe v18.x or before)
#include <sys/stat.h>
#endif

//#define ENABLE_MENU 0
#define DEBUG_KEY_MESSAGES 0

static bool FileExists(std::string strFilename);

// Must keep in sync with Disk_Status_e g_aDiskFullScreenColors
static const DWORD g_aDiskFullScreenColorsLED[ NUM_DISK_STATUS ] =
{
	RGB(  0,  0,  0), // DISK_STATUS_OFF   BLACK
	RGB(  0,255,  0), // DISK_STATUS_READ  GREEN
	RGB(255,  0,  0), // DISK_STATUS_WRITE RED
	RGB(255,128,  0)  // DISK_STATUS_PROT  ORANGE
//	RGB(  0,  0,255)  // DISK_STATUS_PROT  -blue-
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
	HGDIOBJ hOldBrush = SelectObject(dc,GetStockObject(NULL_BRUSH));
	HGDIOBJ hOldPen = SelectObject(dc, btnshadowpen/*out ? btnshadowpen : btnhighlightpen*/);

	Rectangle(dc, x1, y1, x2, y2);
	/*
	POINT pt[3];
	pt[0].x = x1;    pt[0].y = y2-1;
	pt[1].x = x2-1;  pt[1].y = y2-1;
	pt[2].x = x2-1;  pt[2].y = y1; 
	Polyline(dc,(LPPOINT)&pt,3);
	SelectObject(dc,(out == 1) ? btnhighlightpen : btnshadowpen);
	pt[1].x = x1;    pt[1].y = y1;
	pt[2].x = x2;    pt[2].y = y1;
	Polyline(dc,(LPPOINT)&pt,3);
	*/
	SelectObject(dc, hOldPen);
	SelectObject(dc, hOldBrush);
}

typedef BOOL WINAPI TransparentBltProc(
	HDC hdcDest,
	int xoriginDest,
	int yoriginDest,
	int wDest,
	int hDest,
	HDC hdcSrc,
	int xoriginSrc,
	int yoriginSrc,
	int wSrc,
	int hSrc,
	UINT crTransparent);


//===========================================================================
void Win32Frame::DrawBitmapRect(HDC dc, int x, int y, LPRECT rect, HBITMAP bitmap, BOOL transparent)
{
	HDC memdc = CreateCompatibleDC(dc);
	HGDIOBJ hOldBitmap = SelectObject(memdc, bitmap);

	static HMODULE hMsImg32 = NULL;
	static TransparentBltProc* pTransparentBlt = NULL;
	static BOOL transparentBltLoaded = false;

	if (transparent && !transparentBltLoaded)
	{
		transparentBltLoaded = true;

		hMsImg32 = ::LoadLibrary("msimg32.dll");
		if (hMsImg32 != NULL)
			pTransparentBlt = (TransparentBltProc*) ::GetProcAddress(hMsImg32, "TransparentBlt");
	}

	if (transparent && pTransparentBlt != NULL)
	{
		BITMAP bm;
		GetObject(bitmap, sizeof(BITMAP), &bm);

		pTransparentBlt(dc, x, y,
			rect->right + 1 - rect->left,
			rect->bottom + 1 - rect->top,
			memdc,
			rect->left,
			rect->top,
			bm.bmWidth - rect->left,
			bm.bmHeight - rect->top,
			GetSysColor(COLOR_BTNFACE));
	}
	else
	{
		BitBlt(dc, x, y,
			rect->right + 1 - rect->left,
			rect->bottom + 1 - rect->top,
			memdc,
			rect->left,
			rect->top,
			SRCCOPY);
	}

	SelectObject(memdc, hOldBitmap);
	DeleteDC(memdc);
}


//===========================================================================
void Win32Frame::FillSolidRect(HDC dc, int x1, int y1, int x2, int y2, COLORREF color)
{
	COLORREF hOldColor = SetBkColor(dc, color);
	int oldMode = SetBkMode(dc, OPAQUE);

	RECT rc { x1, y1, x2, y2 };
	ExtTextOut(dc, 0, 0, ETO_OPAQUE | ETO_CLIPPED, &rc, NULL, 0, NULL);

	SetBkMode(dc, oldMode);
	SetBkColor(dc, hOldColor);
}


static COLORREF ShadePixels(COLORREF srcPixel, COLORREF dstPixel, int percent)
{
	int ipercent = 100 - percent;

	return RGB((GetBValue(srcPixel) * percent + (GetRValue(dstPixel)) * ipercent) / 100,
		(GetGValue(srcPixel) * percent + (GetGValue(dstPixel)) * ipercent) / 100,
		(GetRValue(srcPixel) * percent + (GetBValue(dstPixel)) * ipercent) / 100);

}

//===========================================================================
void Win32Frame::DrawButton(HDC passdc, int number) 
{
	FrameReleaseDC();
	HDC dc = (passdc ? passdc : GetDC(g_hFrameWindow));

	RECT rc = GetButtonRect(number);
	RECT rectImage = { 1, 1, 40, 40 };

	int x = rc.left;
	int y = rc.top;

	int imgX = x + (BUTTONCX - (rectImage.right - rectImage.left)) / 2;
	int imgY = y + (BUTTONCY - (rectImage.bottom - rectImage.top)) / 2;

	if (number == buttondown)
	{
		Draw3dRect(dc, x + 1, y + 1, x + BUTTONCX - 1, y + BUTTONCY - 1, 0);
		FillSolidRect(dc, x + 2, y + 2, x + BUTTONCX - 2, y + BUTTONCY - 2, GetSysColor(COLOR_3DLIGHT));
		DrawBitmapRect(dc, imgX + 1, imgY + 1, &rectImage, buttonbitmap[number], TRUE);
	}
	else
	{
		BOOL trans = FALSE;

		if (number == buttonover)
		{
			FillSolidRect(dc, x + 2, y + 2, x + BUTTONCX - 2, y + BUTTONCY - 2, ShadePixels(GetSysColor(COLOR_BTNFACE), GetSysColor(COLOR_BTNHIGHLIGHT), 50));
			Draw3dRect(dc, x + 1, y + 1, x + BUTTONCX - 1, y + BUTTONCY - 1, 1);
			trans = TRUE;
		}
		else if ((number == BTN_DEBUG && g_nAppMode == MODE_DEBUG) || (number == BTN_FULLSCR && g_bIsFullScreen)) // Checked ?
		{
			FillSolidRect(dc, x + 2, y + 2, x + BUTTONCX - 2, y + BUTTONCY - 2, GetSysColor(COLOR_3DLIGHT));
			Draw3dRect(dc, x + 1, y + 1, x + BUTTONCX - 1, y + BUTTONCY - 1, 0);
			trans = TRUE;
		}
		else if (!passdc)
			FillSolidRect(dc, x + 1, y + 1, x + BUTTONCX - 1, y + BUTTONCY - 1, GetSysColor(COLOR_BTNFACE));

		DrawBitmapRect(dc, imgX, imgY, &rectImage, buttonbitmap[number], trans);
	}
	
	if (number == BTN_DRIVE1 || number == BTN_DRIVE2)
	{
		int  offset = number == buttondown ? 1 : 0;

		RECT rect = { x + offset + 3,
					 y + offset + 31,
					 x + offset + 42,
					 y + offset + 42 };

		rect = rc;
		::InflateRect(&rect, 3, 3);

		SelectObject(dc, smallfont);
		SetTextColor(dc, RGB(0, 0, 0));
		SetTextAlign(dc, TA_CENTER | TA_TOP);
		SetBkMode(dc, TRANSPARENT);

		LPCTSTR pszBaseName = (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
			? dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).GetBaseName(number - BTN_DRIVE1).c_str()
			: "";

		ExtTextOut(dc, rect.left + offset + BUTTONCX / 2 + 2, rect.bottom - 16 + offset, ETO_CLIPPED, &rect,
			pszBaseName,
			MIN(8, _tcslen(pszBaseName)),
			NULL);
	}

	if (!passdc)
		ReleaseDC(g_hFrameWindow, dc);
}

//===========================================================================

// NB. x=y=0 means erase only
void Win32Frame::DrawCrosshairs (int x, int y) 
{
	if (x && y)
	{
		FrameReleaseDC();

		HDC dc = GetDC(g_hFrameWindow);

		RECT rcVid = GetVideoRect();

		#define LINE(x1,y1,x2,y2) MoveToEx(dc,x1,y1,NULL); LineTo(dc,x2,y2);
		#define SZ 4

		HGDIOBJ hPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
		HGDIOBJ hOld = SelectObject(dc, hPen);

		LINE(x, rcVid.top, x, rcVid.top + SZ);
		LINE(x, rcVid.bottom - SZ, x, rcVid.bottom);

		LINE(rcVid.left, y, rcVid.left + SZ, y);
		LINE(rcVid.right - SZ, y, rcVid.right, y);

		SelectObject(dc, hOld);
		DeleteObject(hPen);

		ReleaseDC(g_hFrameWindow, dc);
	}
}

//===========================================================================
void Win32Frame::DrawFrameWindow (bool bPaintingWindow/*=false*/)
{
	FrameReleaseDC();

	PAINTSTRUCT ps;
	HDC         dc = bPaintingWindow
		? BeginPaint(g_hFrameWindow,&ps)
		: GetDC(g_hFrameWindow);

	if (!g_bIsFullScreen || m_fullScreenToolbarVisible)
	{
		// DRAW THE TOOLBAR BACKGROUND
		RECT rc = GetToolbarRect();
		FillSolidRect(dc, rc.left, rc.top, rc.right, rc.bottom, GetSysColor(COLOR_BTNFACE));

		// DRAW THE TOOLBAR BUTTONS
		int iButton = BUTTONS;
		while (iButton--)
			DrawButton(dc, iButton);
	}

	// DRAW THE STATUS AREA
	DrawStatusArea(dc, DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);

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

bool Win32Frame::GetIntegerScale(void)
{
	return g_bIntegerScale;
}

void Win32Frame::SetIntegerScale(bool bShow)
{
	g_bIntegerScale = bShow;
}

bool Win32Frame::GetStretchVideo(void)
{
	return g_bStretchVideo;
}

void Win32Frame::SetStretchVideo(bool bShow)
{
	g_bStretchVideo = bShow;
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
void Win32Frame::FrameDrawDiskLEDS(HDC passdc)
{
	if (g_bIsFullScreen && !m_fullScreenToolbarVisible)
		return;

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
	HDC  dc = (passdc ? passdc : GetDC(g_hFrameWindow));

	RECT rcStatus = GetStatusRect(0);
	int  x = rcStatus.left;
	int  y = rcStatus.top;

	RECT rDiskLed = { 0, 0, 8, 8 };

	int offset = (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2 ? 0 : 6) + IS_APPLE2 ? 8 : 0;

	DrawBitmapRect(dc, x + 13, y + yOffsetSlot6LEDs + offset, &rDiskLed, g_hDiskWindowedLED[g_eStatusDrive1], FALSE);
	DrawBitmapRect(dc, x + 32, y + yOffsetSlot6LEDs + offset, &rDiskLed, g_hDiskWindowedLED[g_eStatusDrive2], FALSE);
	
	if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
	{
		Disk_Status_e eDrive1StatusSlot5 = DISK_STATUS_OFF;
		Disk_Status_e eDrive2StatusSlot5 = DISK_STATUS_OFF;
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT5)).GetLightStatus(&eDrive1StatusSlot5, &eDrive2StatusSlot5);
	
		DrawBitmapRect(dc, x + 13, y + yOffsetHardDiskLED + smallfontHeight + 1, &rDiskLed, g_hDiskWindowedLED[eDrive1StatusSlot5], FALSE);
		DrawBitmapRect(dc, x + 32, y + yOffsetHardDiskLED + smallfontHeight + 1, &rDiskLed, g_hDiskWindowedLED[eDrive2StatusSlot5], FALSE);
	}
	
	if (!passdc)
		ReleaseDC(g_hFrameWindow, dc);
}

void Win32Frame::FrameDrawDiskStatus()
{
	FrameDrawDiskStatus((HDC)0);
}

void Win32Frame::GetTrackSector(UINT slot, int& drive1Track, int& drive2Track, int& activeFloppy)
{
	//        DOS3.3   ProDOS
	// Slot   $B7E9    $BE3C(DEFSLT=Default Slot)      ; ref: Beneath Apple ProDOS 8-3
	// Drive  $B7EA    $BE3D(DEFDRV=Default Drive)     ; ref: Beneath Apple ProDOS 8-3
	// Track  $B7EC    LC1 $D356
	// Sector $B7ED    LC1 $D357
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
		const int nProDOSslot = mem[0xBE3C];
		const int nProDOStrack = *MemGetMainPtr(0xC356); // LC1 $D356
		const int nProDOSsector = *MemGetMainPtr(0xC357); // LC1 $D357

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
	RECT rcStatus = GetStatusRect(slot == SLOT6 ? 1 : 2);
	if (rcStatus.left < 0)
		return;

	const int x = rcStatus.left;
	const int y = rcStatus.top;

	// Erase background (TrackInfo + SectorInfo)
	//FillSolidRect(dc, x + 1, y - 1, x + BUTTONCX - 1, y + BUTTONCY - 1, GetSysColor(COLOR_BTNFACE));
	// Draw3dRect(dc, x, y, x + BUTTONCX, y + BUTTONCY, 0);

	SelectObject(dc, smallfont);
	SetTextAlign(dc, TA_LEFT | TA_TOP);
	SetTextColor(dc, RGB(0, 0, 0));
	SetBkMode(dc, OPAQUE);
	SetBkColor(dc, GetSysColor(COLOR_BTNFACE));

	std::string strTrackDrive1, strSectorDrive1, strTrackDrive2, strSectorDrive2;
	CreateTrackSectorStrings(drive1Track, drive1Sector, strTrackDrive1, strSectorDrive1);
	CreateTrackSectorStrings(drive2Track, drive2Sector, strTrackDrive2, strSectorDrive2);

	std::string text;

	text = slot == SLOT6 ? "Slot 6:" : "Slot 5:";
	TextOut(dc, x + 5, y + 5, text.c_str(), text.length());

	text = "T" + strTrackDrive1 + "  ";
	TextOut(dc, x + 5, y + 7 + smallfontHeight, text.c_str(), text.length());
	text = "S" + strSectorDrive1 + "  ";
	TextOut(dc, x + 5, y + 7 + smallfontHeight + smallfontHeight, text.c_str(), text.length());

	text = "T" + strTrackDrive2 + "  ";
	TextOut(dc, x + 25, y + 7 + smallfontHeight, text.c_str(), text.length());
	text = "S" + strSectorDrive2 + "  ";
	TextOut(dc, x + 25, y + 7 + smallfontHeight + smallfontHeight, text.c_str(), text.length());
}

// Feature Request #201 Show track status
// https://github.com/AppleWin/AppleWin/issues/201
//===========================================================================
void Win32Frame::FrameDrawDiskStatus(HDC passdc)
{
	if (mem == NULL)
		return;

	if (g_nAppMode == MODE_LOGO)
		return;

	if (g_windowMinimized)	// Prevent DC leaks when app window is minimised (GH#820)
		return;

	if (g_bIsFullScreen && !m_fullScreenToolbarVisible)
		return;

	// NB. Only draw Track/Sector if 2x windowed
	if (/*GetViewportScale() <= 1 || */!GetWindowedModeShowDiskiiStatus())
		return;

	int nDrive1Track, nDrive2Track, nActiveFloppy;
	GetTrackSector(SLOT6, nDrive1Track, nDrive2Track, nActiveFloppy);

	// Draw Track/Sector
	FrameReleaseDC();
	HDC dc = (passdc ? passdc : GetDC(g_hFrameWindow));

	DrawTrackSector(dc, SLOT6, nDrive1Track, g_nSector[SLOT6][0], nDrive2Track, g_nSector[SLOT6][1]);

	// Slot 5's Disk II
	if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
	{
		GetTrackSector(SLOT5, nDrive1Track, nDrive2Track, nActiveFloppy);
		DrawTrackSector(dc, SLOT5, nDrive1Track, g_nSector[SLOT5][0], nDrive2Track, g_nSector[SLOT5][1]);
	}	
	
	if (!passdc)
		ReleaseDC(g_hFrameWindow, dc);
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

	if (g_bIsFullScreen && !m_fullScreenToolbarVisible)
		return;

	FrameReleaseDC();
	HDC  dc = (passdc ? passdc : GetDC(g_hFrameWindow));

	RECT rcStatus = GetStatusRect(0);

	int  x = rcStatus.left;
	int  y = rcStatus.top;

	const bool bCaps = KeybGetCapsStatus();

	Disk_Status_e eHardDriveStatus = DISK_STATUS_OFF;
	if (GetCardMgr().QuerySlot(SLOT7) == CT_GenericHDD)
		dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(SLOT7)).GetLightStatus(&eHardDriveStatus);

	int offset = (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2 ? 0 : 6) + IS_APPLE2 ? 8 : 0;

	if (drawflags & DRAW_BACKGROUND)
	{
		// Erase background (Slot6 drive LEDs, HDD LED & Caps)
		FillSolidRect(dc, x + 1, y + 1, x - 2, y + BUTTONCY - 2, GetSysColor(COLOR_BTNFACE));
		// Draw3dRect(dc, x, y, x + BUTTONCX, y + BUTTONCY, 0);

		// Add text for Slot6 drives: "1" & "2"
		SelectObject(dc, smallfont);
		SetTextAlign(dc, TA_CENTER | TA_TOP);
		SetTextColor(dc, RGB(0, 0, 0));
		SetBkMode(dc, TRANSPARENT);

		TextOut(dc, x + 7, y +  yOffsetSlot6LEDNumbers + offset, "1", 1);
		TextOut(dc, x + 27, y + yOffsetSlot6LEDNumbers + offset, "2", 1);

		// Add text for Slot7 harddrive: "H"
		if (!IS_APPLE2)
			TextOut(dc, x + 7, y + yOffsetHardDiskLED + offset, TEXT("H"), 1);
		
		if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
		{
			TextOut(dc, x + 7, y + yOffsetHardDiskLED + smallfontHeight + 1, "1", 1);
			TextOut(dc, x + 27, y + yOffsetHardDiskLED + smallfontHeight + 1, "2", 1);
		}
	}

	if (drawflags & DRAW_LEDS)
	{
		FrameDrawDiskLEDS(dc);

		if (drawflags & DRAW_DISK_STATUS)
			FrameDrawDiskStatus(dc);

		if (!IS_APPLE2)
		{
			RECT rCapsLed = { 0, 0, 10, 12 }; // HACK: HARD-CODED bitmaps size

			switch (g_Apple2Type)
			{
			case A2TYPE_APPLE2:
			case A2TYPE_APPLE2PLUS:
			case A2TYPE_APPLE2E:
			case A2TYPE_APPLE2EENHANCED:
			default: DrawBitmapRect(dc, x + 32, y + yOffsetCapsLock + offset, &rCapsLed, g_hCapsLockBitmap[bCaps != 0], FALSE); break;
			case A2TYPE_PRAVETS82:
			case A2TYPE_PRAVETS8M: DrawBitmapRect(dc, x + 32, y + yOffsetCapsLock + offset, &rCapsLed, g_hCapsBitmapP8[bCaps != 0], FALSE); break; // TODO: FIXME: Shouldn't one of these use g_hCapsBitmapLat ??
			case A2TYPE_PRAVETS8A: DrawBitmapRect(dc, x + 32, y + yOffsetCapsLock + offset, &rCapsLed, g_hCapsBitmapP8[bCaps != 0], FALSE); break;
			}

			RECT rDiskLed = { 0, 0, 8, 8 };
			DrawBitmapRect(dc, x + 13, y + yOffsetHardDiskLED + offset, &rDiskLed, g_hDiskWindowedLED[eHardDriveStatus], FALSE);
		}
	}

	if (drawflags & DRAW_TITLE)
	{
		GetAppleWindowTitle(); // SetWindowText() // WindowTitle
		SendMessage(g_hFrameWindow, WM_SETTEXT, 0, (LPARAM)g_pAppTitle.c_str());
	}

	if (drawflags & DRAW_BUTTON_DRIVES)
	{
		DrawButton(dc, BTN_DRIVE1);
		DrawButton(dc, BTN_DRIVE2);
	}

	if (!passdc)
		ReleaseDC(g_hFrameWindow, dc);
}

//===========================================================================
void Win32Frame::EraseButton(int number) 
{
	RECT rect = GetButtonRect(number);
	InvalidateRect(g_hFrameWindow, &rect, 1);
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
	case WM_GETMINMAXINFO:
		{
			SIZE sz = GetWidthHeight(1);

			LPMINMAXINFO pMMI = (LPMINMAXINFO)lparam;
			pMMI->ptMinTrackSize.x = sz.cx;
			pMMI->ptMinTrackSize.y = sz.cy;
		}
		break;
		
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

		OnSizeChanged();
		InvalidateRect(g_hFrameWindow, NULL, 1);
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
      if (!g_bRestart) {
		GetCardMgr().Destroy();
      }
      CpuDestroy();
      MemDestroy();
      SpkrDestroy();
      Destroy();
      MB_Destroy();
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
					EraseButton(BTN_DRIVE1);

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
			RECT rect = GetButtonRect(BTN_DRIVE2);
			const int iDrive = PtInRect(&rect,point) ? DRIVE_2 : DRIVE_1;
			ImageError_e Error = disk2Card.InsertDisk(iDrive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
			if (Error == eIMAGE_ERROR_NONE)
			{
				if (!g_bIsFullScreen)
					EraseButton(PtInRect(&rect,point) ? BTN_DRIVE2 : BTN_DRIVE1);

				rect = GetButtonRect(BTN_DRIVE1);				
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
			buttondown = (wparam - VK_F1 - 1) % BUTTONS;			
			if (g_bIsFullScreen && (buttonover != -1)) {
				if (buttonover != buttondown)
				EraseButton(buttonover);
				buttonover = -1;
			}
			EraseButton(buttondown);
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
		if (wparam >= VK_F1 && wparam <= VK_F8)
		{
			int btn = (wparam - VK_F1 - 1) % BUTTONS;
			if (buttondown == btn)
			{
				buttondown = -1;
				EraseButton(btn);

				const int iButton = btn;
				if (KeybGetCtrlStatus() && (wparam == VK_F3 || wparam == VK_F4))	// Ctrl+F3/F4 for drive pop-up menu (GH#817)
				{
					RECT rcButton = GetButtonRect(iButton);
										
					// Center point
					POINT pt;
					pt.x = rcButton.left + (rcButton.right - rcButton.left) / 2;
					pt.y = rcButton.top + (rcButton.bottom - rcButton.top) / 2;

					const int iDrive = btn + 2;
					ProcessDiskPopupMenu(window, pt, iDrive);

					FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
					EraseButton(iButton);
				}
				else
				{
					ProcessButtonClick(iButton, true);
				}
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

		int btn = HitTestButton(x, y);
		if (btn >= 0)
		{
			buttonactive = buttondown = btn; // buttondown = (y - buttony - 1) / BUTTONCY;
			EraseButton(buttonactive);
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
        else if ((JoyUsingMouse() && ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))))
		{			
			RECT rcVid = GetVideoRect();
			if (::PtInRect(&rcVid, POINT{ x, y }))
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
		if (buttonactive != -1) 
		{
			ReleaseCapture();
			if (buttondown == buttonactive)
			{
				buttondown = -1;
				DrawButton(NULL, buttonactive);
				ProcessButtonClick(buttonactive, true);
			}
			int old = buttonactive;
			buttonactive = -1;
			EraseButton(old);
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

	case WM_MOUSELEAVE:
		if (buttonover != -1)
		{
			int old = buttonover;
			buttonover = -1;
			EraseButton(old);
		}
		break;

    case WM_MOUSEMOVE: 
	{
		// MSDN: "WM_MOUSEMOVE message" : Do not use the LOWORD or HIWORD macros to extract the x- and y- coordinates...
		int x = GET_X_LPARAM(lparam);
		int y = GET_Y_LPARAM(lparam);

		RECT rc = GetToolbarRect();
		BOOL fullScreenTb = IsFullScreen() && (buttonactive >= 0 || ::PtInRect(&rc, POINT{ x, y }));
		if (m_fullScreenToolbarVisible != fullScreenTb)
		{
			m_fullScreenToolbarVisible = fullScreenTb;
			InvalidateRect(g_hFrameWindow, &rc, TRUE);
		}

		int newover = HitTestButton(x, y);
		if (buttonactive != -1)
		{
			int newdown = (newover == buttonactive) ? buttonactive : -1;
			if (newdown != buttondown)
			{
				buttondown = newdown;
				EraseButton(buttonactive);
			}
		}
		else if (newover != buttonover)
		{
			if (buttonover != -1)
				EraseButton(buttonover);				

			buttonover = newover;

			if (buttonover != -1)
			{
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = window;
				::_TrackMouseEvent(&tme);

				DrawButton(NULL, buttonover); 
				//EraseButton(buttonover);
			}
		}
		else if (g_bUsingCursor && !GetCardMgr().IsMouseCardInstalled())
		{
			DrawCrosshairs(x, y);
			RECT rc = GetVideoRect();
			JoySetPosition(x - rc.left, rc.right, y - rc.top, rc.bottom);
		}
		else if (GetCardMgr().IsMouseCardInstalled() && GetCardMgr().GetMouseCard()->IsActiveAndEnabled() && (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_STEPPING))
		{
			if (g_bLastCursorInAppleViewport)
				break;

			// Outside Apple viewport

			RECT rc = GetVideoRect();
			const int iAppleScreenMaxX = rc.right - rc.left - 1;
			const int iAppleScreenMaxY = rc.bottom - rc.top - 1;
			const int iBoundMinX = rc.left;
			const int iBoundMaxX = iAppleScreenMaxX;
			const int iBoundMinY = rc.top;
			const int iBoundMaxY = iAppleScreenMaxY;

			int iOutOfBoundsX = 0, iOutOfBoundsY = 0;
			if (x < iBoundMinX)	iOutOfBoundsX = -1;
			if (x > iBoundMaxX)	iOutOfBoundsX = 1;
			if (y < iBoundMinY)	iOutOfBoundsY = -1;
			if (y > iBoundMaxY)	iOutOfBoundsY = 1;

			UpdateMouseInAppleViewport(iOutOfBoundsX, iOutOfBoundsY, x, y);
		}

		FullScreenRevealCursor();

		RelayEvent(WM_MOUSEMOVE, wparam, lparam);
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

			driveTooltip = "";

			switch (pInfo->hdr.idFrom)
			{
			case BTN_RUN:
				driveTooltip = "Run (F2)";
				break;
			case BTN_HELP:
				driveTooltip = "Help (F1)";
				break;
			case BTN_DRIVESWAP:
				driveTooltip = "Swap drives (F5)";
				break;
			case BTN_FULLSCR:
				driveTooltip = "Fullscreen (F6)";
				break;
			case BTN_DEBUG:
				driveTooltip = "Debugger (F7)";
				break;
			case BTN_SETUP:
				driveTooltip = "Setup (F8)";
				break;
			case BTN_DRIVE1:
			case BTN_DRIVE2:
			{
				SendMessage(pInfo->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 150);

				Disk2InterfaceCard* pDisk2Slot5 = NULL, * pDisk2Slot6 = NULL;

				if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
					pDisk2Slot5 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT5));
				if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
					pDisk2Slot6 = dynamic_cast<Disk2InterfaceCard*>(GetCardMgr().GetObj(SLOT6));

				int drive = pInfo->hdr.idFrom - BTN_DRIVE1;
				std::string slot5 = pDisk2Slot5 ? pDisk2Slot5->GetFullDiskFilename(drive) : "";
				std::string slot6 = pDisk2Slot6 ? pDisk2Slot6->GetFullDiskFilename(drive) : "";

				if (pDisk2Slot5)
				{
					if (slot6.empty()) slot6 = "<empty>";
					if (slot5.empty()) slot5 = "<empty>";
					slot6 = std::string("Slot6: ") + slot6;
					slot5 = std::string("Slot5: ") + slot5;
				}

				std::string join = (!slot6.empty() && !slot5.empty()) ? "\n" : "";
				driveTooltip = slot6 + join + slot5;
			}
			case TTID_SLOT6_TRK_SEC_INFO:
			case TTID_SLOT5_TRK_SEC_INFO:
			{
				if (g_nAppMode == MODE_LOGO || !GetWindowedModeShowDiskiiStatus())
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
			}
			}

			if (!driveTooltip.empty())
				pInfo->lpszText = (LPTSTR)driveTooltip.c_str();
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
		if (message == WM_RBUTTONUP) // HACK: BUTTON_NONE
		{
			int x = LOWORD(lparam);
			int y = HIWORD(lparam);

			int btn = HitTestButton(x, y);
			if (btn >= 0)
			{
				const int iButton = btn;
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
					EraseButton(iButton);
				}			
			}			
			else
			{
				RECT rcToolbar = GetToolbarRect();
				POINT pt = { x, y };
				if (::PtInRect(&rcToolbar, pt))
					ProcessToolbarPopupMenu(window, pt);
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
		nOldViewportScale = nOldViewportScale == 1 ? 2 : 1;
		FrameResizeWindow(nOldViewportScale);	// Toggle between 1x and 2x
		// REGSAVE(TEXT(REGVALUE_WINDOW_SCALE), nOldViewportScale);
	}
	else
	{
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
				EraseButton(button);
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
		  InvalidateRect(g_hFrameWindow, NULL, 1); // In case Integer scale changed
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

	// Check menu depending on current floppy protection
	{
		int iMenuItem = ID_DISKMENU_WRITEPROTECTION_OFF;
		if (disk2Card.GetProtect(iDrive))
			iMenuItem = ID_DISKMENU_WRITEPROTECTION_ON;

		CheckMenuItem(hmenu, iMenuItem, MF_CHECKED);
	}

	if (disk2Card.IsDriveEmpty(iDrive))
		EnableMenuItem(hmenu, ID_DISKMENU_EJECT, MF_GRAYED);

	if (disk2Card.GetProtect(iDrive))
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

	// Destroy the menu.
	BOOL bRes = DestroyMenu(hmenu);
	_ASSERT(bRes);
}

void Win32Frame::ProcessToolbarPopupMenu(HWND hWnd, POINT point)
{
	//  Load the menu template containing the shortcut menu from the
//  application's resources.
	HMENU hmenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_TOOLBAR));	// menu template
	if (hmenu == NULL)
		return;

	ClientToScreen(hWnd, &point);

	switch (m_toolbarPosition)
	{
	case ToolbarPosition::TOP:
		CheckMenuItem(hmenu, ID_TOOLBAR_TOP, MF_CHECKED);
		break;
	case ToolbarPosition::BOTTOM:
		CheckMenuItem(hmenu, ID_TOOLBAR_BOTTOM, MF_CHECKED);
		break;
	case ToolbarPosition::LEFT:
		CheckMenuItem(hmenu, ID_TOOLBAR_LEFT, MF_CHECKED);
		break;
	case ToolbarPosition::RIGHT:
		CheckMenuItem(hmenu, ID_TOOLBAR_RIGHT, MF_CHECKED);
		break;
	}

	// Get the first shortcut menu in the menu template.
	// This is the menu that TrackPopupMenu displays.
	HMENU hmenuTrackPopup = GetSubMenu(hmenu, 0);

	int iCommand = TrackPopupMenu(hmenuTrackPopup, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD, point.x, point.y , 0 , hWnd, NULL);
	switch (iCommand)
	{
	case ID_TOOLBAR_TOP:
		m_toolbarPosition = ToolbarPosition::TOP;		
		break;
	case ID_TOOLBAR_BOTTOM:
		m_toolbarPosition = ToolbarPosition::BOTTOM;
		break;
	case ID_TOOLBAR_LEFT:
		m_toolbarPosition = ToolbarPosition::LEFT;
		break;
	case ID_TOOLBAR_RIGHT:
		m_toolbarPosition = ToolbarPosition::RIGHT;
		break;
	}

	RegSaveValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_TOOLBAR), 1, (DWORD) m_toolbarPosition);
	InvalidateRect(g_hFrameWindow, NULL, 1);
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

		DWORD dwFlags = 0;
		LONG res = ChangeDisplaySettings(&devMode, dwFlags);
		m_changedDisplaySettings = true;
	}

	//
	buttonover = -1;

	g_main_window_saved_style = GetWindowLong(g_hFrameWindow, GWL_STYLE);
	g_main_window_saved_exstyle = GetWindowLong(g_hFrameWindow, GWL_EXSTYLE);
	GetWindowRect(g_hFrameWindow, &g_main_window_saved_rect);
	SetWindowLong(g_hFrameWindow, GWL_STYLE  , g_main_window_saved_style   & ~(WS_CAPTION | WS_THICKFRAME));
	SetWindowLong(g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
	
	MONITORINFO monitor_info;
	monitor_info.cbSize = sizeof(monitor_info);
	GetMonitorInfo(MonitorFromWindow(g_hFrameWindow, MONITOR_DEFAULTTONEAREST), &monitor_info);

	int left = monitor_info.rcMonitor.left;
	int top  = monitor_info.rcMonitor.top;
	int width  = monitor_info.rcMonitor.right  - monitor_info.rcMonitor.left;
	int height = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;

	SetWindowPos(g_hFrameWindow, NULL, left, top, (int)width, (int)height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

	g_bIsFullScreen = true;
	m_fullScreenToolbarVisible = false;

	OnSizeChanged();
	InvalidateRect(g_hFrameWindow, NULL, 1);

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

	SetWindowLong(g_hFrameWindow, GWL_STYLE, g_main_window_saved_style);
	SetWindowLong(g_hFrameWindow, GWL_EXSTYLE, g_main_window_saved_exstyle);
	SetWindowPos(g_hFrameWindow, NULL,
		g_main_window_saved_rect.left,
		g_main_window_saved_rect.top,
		g_main_window_saved_rect.right - g_main_window_saved_rect.left,
		g_main_window_saved_rect.bottom - g_main_window_saved_rect.top,
		SWP_SHOWWINDOW);

	g_bIsFullScreen = false;
	m_fullScreenToolbarVisible = false;
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
		RECT rect = GetVideoRect();
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
	RECT rc = GetClientArea();
	int scale = max(1, (rc.bottom - rc.top + 32) / GetVideo().GetFrameBufferHeight()); // Add 32 pixels margin
	return scale;
}

void Win32Frame::SetupTooltipControls(void)
{
	// Remove the tooltips for the old window size
	for (int i = SendMessage(tooltipwindow, TTM_GETTOOLCOUNT, 0, 0); i > 0; i--)
	{
		TOOLINFO ti;
		ti.cbSize = sizeof(ti);
		if (SendMessage(tooltipwindow, TTM_ENUMTOOLS, i - 1, (LPARAM)&ti))
			SendMessage(tooltipwindow, TTM_DELTOOL, 0, (LPARAM)&ti);
	}

	TOOLINFO toolinfo;
	toolinfo.cbSize = sizeof(toolinfo);
	toolinfo.hwnd = g_hFrameWindow;
	toolinfo.uFlags = TTF_CENTERTIP;
	toolinfo.hinst = g_hInstance;
	toolinfo.lpszText = LPSTR_TEXTCALLBACK;

	for (int i = 0; i < BUTTONS; i++)
	{
		toolinfo.uId = i;
		toolinfo.rect = GetButtonRect(i);
		SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
	}

	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
	{
		toolinfo.uId = TTID_SLOT6_TRK_SEC_INFO;			
		toolinfo.rect = GetStatusRect(1);
		if (toolinfo.rect.left >= 0)
			SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
	}

	if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
	{
		toolinfo.uId = TTID_SLOT5_TRK_SEC_INFO;
		toolinfo.rect = GetStatusRect(2);
		if (toolinfo.rect.left >= 0)
			SendMessage(tooltipwindow, TTM_ADDTOOL, 0, (LPARAM)&toolinfo);
	}
}

// SM_CXPADDEDBORDER is not supported on 2000 & XP, but GetSystemMetrics() returns 0 for unknown values, so this use of SM_CXPADDEDBORDER works on 2000 & XP too:
// http://msdn.microsoft.com/en-nz/library/windows/desktop/ms724385(v=vs.85).aspx
// NB. GetSystemMetrics(SM_CXPADDEDBORDER) returns 0 for Win7, when built with VS2008 (see GH#571)

SIZE Win32Frame::GetWidthHeight(int scaleFactor)
{
#if RENDER_BORDERMARGIN
	int nViewportCX = GetVideo().GetFrameBufferWidth() * scaleFactor;
	int nViewportCY = GetVideo().GetFrameBufferHeight() * scaleFactor;
#else
	int nViewportCX = GetVideo().GetFrameBufferBorderlessWidth() * scaleFactor;
	int nViewportCY = GetVideo().GetFrameBufferBorderlessHeight() * scaleFactor;
#endif

	RECT rc = GetToolbarRect();

	SIZE sz =
	{
		nViewportCX + GetSystemMetrics(SM_CXFRAME) * 2,
		nViewportCY + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION)
	};

	if (m_toolbarPosition == ToolbarPosition::TOP || m_toolbarPosition == ToolbarPosition::BOTTOM)
		sz.cy += BUTTONCY;
	else 
		sz.cx += BUTTONCX;

	return sz;
}

// Window frame's border size has changed (eg. VidHD added/removed)
void Win32Frame::ResizeWindow(void)
{
	FrameResizeWindow(GetViewportScale());
}

void Win32Frame::FrameResizeWindow(int nNewScale)
{
	RECT rcWindow;
	GetWindowRect(g_hFrameWindow, &rcWindow);

	SIZE sz = GetWidthHeight(nNewScale);

	MoveWindow(g_hFrameWindow, rcWindow.left, rcWindow.top, sz.cx, sz.cy, TRUE);
	UpdateWindow(g_hFrameWindow);
}

void Win32Frame::OnSizeChanged()
{
	// Setup the tooltips for the new window size
	SetupTooltipControls();
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void Win32Frame::FrameCreateWindow(void)
{
	SIZE sz = GetWidthHeight(2);
	if (sz.cx > GetSystemMetrics(SM_CXSCREEN) || sz.cy > GetSystemMetrics(SM_CYSCREEN))
		sz = GetWidthHeight(1);

	DWORD dwToolbar;
	if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_TOOLBAR), 1, &dwToolbar))
		m_toolbarPosition = (ToolbarPosition) dwToolbar;

	// Restore Window X Position
	int nXPos = -1;
	{
		const int nXScreen = GetSystemMetrics(SM_CXSCREEN) - sz.cx;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_X_POS), 1, (DWORD*)&nXPos))
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
		const int nYScreen = GetSystemMetrics(SM_CYSCREEN) - sz.cy;

		if (RegLoadValue(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_WINDOW_Y_POS), 1, (DWORD*)&nYPos))
		{
			if ((nYPos > nYScreen) && !g_bMultiMon)
				nYPos = -1;	// Not fully visible, so default to centre position
		}

		if ((nYPos == -1) && !g_bMultiMon)
			nYPos = nYScreen / 2;
	}

	GetAppleWindowTitle();

	// NB. g_hFrameWindow also set by WM_CREATE - NB. CreateWindow() must synchronously send WM_CREATE
	g_hFrameWindow = CreateWindow(
		TEXT("APPLE2FRAME"),
		g_pAppTitle.c_str(),
		WS_THICKFRAME | WS_CAPTION | WS_SYSMENU |
		WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_VISIBLE,
		nXPos, nYPos, sz.cx, sz.cy,
		HWND_DESKTOP,
		(HMENU)0,
		g_hInstance, NULL );

	InitCommonControls();
	tooltipwindow = CreateWindow(
		TOOLTIPS_CLASS,NULL,TTS_ALWAYSTIP | TTS_NOPREFIX,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT, 
		g_hFrameWindow,
		(HMENU)0,
		g_hInstance,NULL );

	SendMessage(tooltipwindow, TTM_SETDELAYTIME, TTDT_INITIAL, GetDoubleClickTime());
	SendMessage(tooltipwindow, TTM_SETDELAYTIME, TTDT_RESHOW, GetDoubleClickTime() / 2);

	SetupTooltipControls();
	OnSizeChanged();

	_ASSERT(g_TimerIDEvent_100msec == 0);
	g_TimerIDEvent_100msec = SetTimer(g_hFrameWindow, IDEVENT_TIMER_100MSEC, 100, NULL);
	LogFileOutput("FrameCreateWindow: SetTimer(), id=0x%08X\n", g_TimerIDEvent_100msec);
}

//===========================================================================
HDC Win32Frame::FrameGetDC () {
  if (!g_hFrameDC) {
    g_hFrameDC = GetDC(g_hFrameWindow);
  }
  return g_hFrameDC;
}

//===========================================================================
void Win32Frame::FrameReleaseDC () {
  if (g_hFrameDC) {    
    ReleaseDC(g_hFrameWindow,g_hFrameDC);
    g_hFrameDC = (HDC)0;
  }
}

//===========================================================================
void Win32Frame::FrameRefreshStatus (int drawflags) 
{
	InvalidateRect(g_hFrameWindow, NULL, 1);
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

	RECT rc = GetVideoRect();
	int iWindowX = rc.left + (int)(fScaleX * (float)(rc.right - rc.left));
	int iWindowY = rc.top + (int)(fScaleY * (float)(rc.bottom - rc.top));

	POINT pt = { 0, 0 };
	ClientToScreen(g_hFrameWindow, &pt);
	SetCursorPos(pt.x + iWindowX, pt.y + iWindowY);

#if defined(_DEBUG) && 0	// OutputDebugString() when cursor position changes since last time
	static int OldX = 0, OldY = 0;
	int X = pt.x + iWindowX;
	int Y = pt.y + iWindowY;
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

	RECT rc = GetVideoRect();

	if (bLeavingAppleScreen)
	{
		// Set mouse x/y pos to edge of mouse's window
		if (dx < 0) iX = iMinX;
		if (dx > 0) iX = iMaxX;
		if (dy < 0) iY = iMinY;
		if (dy > 0) iY = iMaxY;

		float fScaleX = (float)(iX-iMinX) / ((float)(iMaxX-iMinX));
		float fScaleY = (float)(iY-iMinY) / ((float)(iMaxY-iMinY));
		
		int iWindowX = rc.left + (int)(fScaleX * (float)(rc.right - rc.left)) + dx;
		int iWindowY = rc.top + (int)(fScaleY * (float)(rc.bottom - rc.top)) + dy;

		POINT pt = { 0, 0 };	// top-left
		ClientToScreen(g_hFrameWindow, &pt);
		SetCursorPos(pt.x + iWindowX, pt.y + iWindowY);
		//LogOutput("[MOUSE_LEAVING ] x=%d, y=%d (Scale: x,y=%f,%f; iX,iY=%d,%d)\n", iWindowX, iWindowY, fScaleX, fScaleY, iX, iY);
	}
	else	// Mouse entering Apple screen area
	{
		float fScaleX = (float)x / (float)(rc.right - rc.left);
		float fScaleY = (float)y / (float)(rc.bottom - rc.top);

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

	RECT rc = GetVideoRect();
	int iWindowX = rc.left + (int)(fScaleX * (float)(rc.right - rc.left));
	int iWindowY = rc.top + (int)(fScaleY * (float)(rc.bottom - rc.top));

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
