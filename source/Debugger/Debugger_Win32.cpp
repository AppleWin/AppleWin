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

/* Description: Debugger
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

 // disable warning C4786: symbol greater than 255 character:
 //#pragma warning(disable: 4786)

#include "StdAfx.h"

#include "Debugger_Win32.h"
#include "Debug.h"

#include "../Windows/Win32Frame.h"
#include "../Interface.h"
#include "../Keyboard.h"
#include "../Core.h"
#include "../CPU.h"

// Config - Font __________________________________________________________________________________
// Font

//===========================================================================
static Update_t CmdConfigFontLoad(int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
static Update_t CmdConfigFontSave(int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

// Only for FONT_DISASM_DEFAULT !
static void _UpdateWindowFontHeights(int nFontHeight)
{
	if (nFontHeight)
	{
		int nConsoleTopY = GetConsoleTopPixels(g_nConsoleDisplayLines);

		int nHeight = 0;

		if (g_iFontSpacing == FONT_SPACING_CLASSIC)
		{
			nHeight = nFontHeight + 1;
			g_nDisasmDisplayLines = nConsoleTopY / nHeight;
		}
		else
			if (g_iFontSpacing == FONT_SPACING_CLEAN)
			{
				nHeight = nFontHeight;
				g_nDisasmDisplayLines = nConsoleTopY / nHeight;
			}
			else
				if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
				{
					nHeight = nFontHeight - 1;
					g_nDisasmDisplayLines = (nConsoleTopY + nHeight) / nHeight; // Ceil()
				}

		g_aFontConfig[FONT_DISASM_DEFAULT]._nLineHeight = nHeight;

		//		int nHeightOptimal = (nHeight0 + nHeight1) / 2;
		//		int nLinesOptimal = nConsoleTopY / nHeightOptimal;
		//		g_nDisasmDisplayLines = nLinesOptimal;

		WindowUpdateSizes();
	}
}

//===========================================================================
static Update_t CmdConfigFontMode(int nArgs)
{
	if (nArgs != 2)
		return Help_Arg_1(CMD_CONFIG_FONT);

	int nMode = g_aArgs[2].nValue;

	if ((nMode < 0) || (nMode >= NUM_FONT_SPACING))
		return Help_Arg_1(CMD_CONFIG_FONT);

	g_iFontSpacing = nMode;
	_UpdateWindowFontHeights(g_aFontConfig[FONT_DISASM_DEFAULT]._nFontHeight);

	return UPDATE_CONSOLE_DISPLAY | UPDATE_DISASM;
}

//===========================================================================
bool _CmdConfigFont(int iFont, LPCSTR pFontName, int iPitchFamily, int nFontHeight)
{
	bool bStatus = false;
	HFONT         hFont = (HFONT)0;
	FontConfig_t* pFont = NULL;

	if (iFont < NUM_FONTS)
		pFont = &g_aFontConfig[iFont];
	else
		return bStatus;

	if (pFontName)
	{
		//		int nFontHeight = g_nFontHeight - 1;

		int bAntiAlias = (nFontHeight < 14) ? DEFAULT_QUALITY : ANTIALIASED_QUALITY;

		// Try allow new font
		hFont = CreateFont(
			nFontHeight
			, 0 // Width
			, 0 // Escapement
			, 0 // Orientatin
			, FW_MEDIUM // Weight
			, 0 // Italic
			, 0 // Underline
			, 0 // Strike Out
			, DEFAULT_CHARSET // OEM_CHARSET
			, OUT_DEFAULT_PRECIS
			, CLIP_DEFAULT_PRECIS
			, bAntiAlias // ANTIALIASED_QUALITY // DEFAULT_QUALITY
			, iPitchFamily // HACK: MAGIC #: 4
			, pFontName);

		if (hFont)
		{
			if (iFont == FONT_DISASM_DEFAULT)
				_UpdateWindowFontHeights(nFontHeight);

			_tcsncpy(pFont->_sFontName, pFontName, MAX_FONT_NAME - 1);
			pFont->_sFontName[MAX_FONT_NAME - 1] = 0;

			Win32Frame& win32Frame = Win32Frame::GetWin32Frame();

			HDC hDC = win32Frame.FrameGetDC();

			TEXTMETRIC tm;
			GetTextMetrics(hDC, &tm);

			SIZE  size;
			TCHAR sText[] = "W";
			int   nLen = 1;

			int nFontWidthAvg;
			int nFontWidthMax;

			//			if (! (tm.tmPitchAndFamily & TMPF_FIXED_PITCH)) // Windows has this bitflag reversed!
			//			{	// Proportional font?
			//				bool bStop = true;
			//			}

						// GetCharWidth32() doesn't work with TrueType Fonts
			if (GetTextExtentPoint32(hDC, sText, nLen, &size))
			{
				nFontWidthAvg = tm.tmAveCharWidth;
				nFontWidthMax = size.cx;
			}
			else
			{
				//  Font Name     Avg Max  "W"
				//  Arial           7   8   11
				//  Courier         5  32   11
				//  Courier New     7  14
				nFontWidthAvg = tm.tmAveCharWidth;
				nFontWidthMax = tm.tmMaxCharWidth;
			}

			if (!nFontWidthAvg)
			{
				nFontWidthAvg = 7;
				nFontWidthMax = 7;
			}

			win32Frame.FrameReleaseDC();

			//			DeleteObject( g_hFontDisasm );
			//			g_hFontDisasm = hFont;
			pFont->_hFont = hFont;

			pFont->_nFontWidthAvg = nFontWidthAvg;
			pFont->_nFontWidthMax = nFontWidthMax;
			pFont->_nFontHeight = nFontHeight;

			bStatus = true;
		}
	}
	return bStatus;
}

void FontsInitialize()
{
	for (int iFont = 0; iFont < NUM_FONTS; iFont++ )
	{
		g_aFontConfig[ iFont ]._hFont = NULL;
#if USE_APPLE_FONT
		g_aFontConfig[ iFont ]._nFontHeight   = CONSOLE_FONT_HEIGHT;
		g_aFontConfig[ iFont ]._nFontWidthAvg = CONSOLE_FONT_WIDTH;
		g_aFontConfig[ iFont ]._nFontWidthMax = CONSOLE_FONT_WIDTH;
		g_aFontConfig[ iFont ]._nLineHeight   = CONSOLE_FONT_HEIGHT;
#endif
	}

#if OLD_FONT
	_CmdConfigFont( FONT_INFO          , g_sFontNameInfo   , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_CONSOLE       , g_sFontNameConsole, FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_DISASM_DEFAULT, g_sFontNameDisasm , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // OEM_CHARSET
	_CmdConfigFont( FONT_DISASM_BRANCH , g_sFontNameBranch , DEFAULT_PITCH | FF_DECORATIVE, g_nFontHeight+3); // DEFAULT_CHARSET
#endif
	_UpdateWindowFontHeights( g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontHeight );
}

void FontsDestroy()
{
	for (int iFont = 0; iFont < NUM_FONTS; iFont++)
	{
		DeleteObject(g_aFontConfig[iFont]._hFont);
		g_aFontConfig[iFont]._hFont = NULL;
	}
}

//===========================================================================
Update_t CmdConfigGetFont(int nArgs)
{
	if (!nArgs)
	{
		for (int iFont = 0; iFont < NUM_FONTS; iFont++)
		{
			ConsoleBufferPushFormat("  Font: %-20s  A:%2d  M:%2d",
				g_aFontConfig[iFont]._sFontName,
				g_aFontConfig[iFont]._nFontWidthAvg,
				g_aFontConfig[iFont]._nFontWidthMax);
		}
		return ConsoleUpdate();
	}

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdConfigFont(int nArgs)
{
	int iArg;

	if (!nArgs)
		return CmdConfigGetFont(nArgs);
	else
		if (nArgs <= 2) // nArgs
		{
			iArg = 1;

			// FONT * is undocumented, like VERSION *
			if ((!_tcscmp(g_aArgs[iArg].sArg, g_aParameters[PARAM_WILDSTAR].m_sName)) ||
				(!_tcscmp(g_aArgs[iArg].sArg, g_aParameters[PARAM_MEM_SEARCH_WILD].m_sName)))
			{
				ConsoleBufferPushFormat("Lines: %d  Font Px: %d  Line Px: %d"
					, g_nDisasmDisplayLines
					, g_aFontConfig[FONT_DISASM_DEFAULT]._nFontHeight
					, g_aFontConfig[FONT_DISASM_DEFAULT]._nLineHeight);
				ConsoleBufferToDisplay();
				return UPDATE_CONSOLE_DISPLAY;
			}

			int iFound;
			int nFound;

			nFound = FindParam(g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END);
			if (nFound)
			{
				switch (iFound)
				{
				case PARAM_LOAD:
					return CmdConfigFontLoad(nArgs);
					break;
				case PARAM_SAVE:
					return CmdConfigFontSave(nArgs);
					break;
					// TODO: FONT SIZE #
					// TODO: AA {ON|OFF}
				default:
					break;
				}
			}

			nFound = FindParam(g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_FONT_BEGIN, _PARAM_FONT_END);
			if (nFound)
			{
				if (iFound == PARAM_FONT_MODE)
					return CmdConfigFontMode(nArgs);
			}

			return CmdConfigSetFont(nArgs);
		}

	return Help_Arg_1(CMD_CONFIG_FONT);
}

//===========================================================================
void DebuggerMouseClick(int x, int y)
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	KeybUpdateCtrlShiftStatus();
	int iAltCtrlShift = 0;
	iAltCtrlShift |= KeybGetAltStatus() ? 1 << 0 : 0;
	iAltCtrlShift |= KeybGetCtrlStatus() ? 1 << 1 : 0;
	iAltCtrlShift |= KeybGetShiftStatus() ? 1 << 2 : 0;

	// GH#462 disasm click #
	if (iAltCtrlShift != g_bConfigDisasmClick)
		return;

	Win32Frame& win32Frame = Win32Frame::GetWin32Frame();

	int nFontWidth = g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidthAvg * win32Frame.GetViewportScale();
	int nFontHeight = g_aFontConfig[FONT_DISASM_DEFAULT]._nLineHeight * win32Frame.GetViewportScale();

	// do picking

	// NB. Full-screen + VidHD isn't centred yet
	const int nOffsetX = win32Frame.IsFullScreen() ? win32Frame.GetFullScreenOffsetX() : win32Frame.Get3DBorderWidth() + GetVideo().GetFrameBufferCentringOffsetX() * win32Frame.GetViewportScale();
	const int nOffsetY = win32Frame.IsFullScreen() ? win32Frame.GetFullScreenOffsetY() : win32Frame.Get3DBorderHeight();

	const int nOffsetInScreenX = x - nOffsetX;
	const int nOffsetInScreenY = y - nOffsetY;

	if (nOffsetInScreenX < 0 || nOffsetInScreenY < 0)
		return;

	int cx = nOffsetInScreenX / nFontWidth;
	int cy = nOffsetInScreenY / nFontHeight;

#if _DEBUG
	ConsoleDisplayPushFormat("x:%d y:%d  cx:%d cy:%d", x, y, cx, cy);
	DebugDisplay();
#endif

	if (g_iWindowThis == WINDOW_CODE)
	{
		// Display_AssemblyLine -- need Tabs

		if (g_bConfigDisasmAddressView)
		{
			// HACK: hard-coded from DrawDisassemblyLine::aTabs[] !!!
			if (cx < 4) // ####
			{
				g_bConfigDisasmAddressView ^= true;
				DebugDisplay();
			}
			else
				if (cx == 4) //    :
				{
					g_bConfigDisasmAddressColon ^= true;
					DebugDisplay();
				}
				else         //      AD 00 00
					if ((cx > 4) && (cx <= 13))
					{
						g_bConfigDisasmOpcodesView ^= true;
						DebugDisplay();
					}

		}
		else
		{
			if (cx == 0) //   :
			{
				// Three-way state
				//   "addr:"
				//   ":"
				//   " "
				g_bConfigDisasmAddressColon ^= true;
				if (g_bConfigDisasmAddressColon)
				{
					g_bConfigDisasmAddressView ^= true;
				}
				DebugDisplay();
			}
			else
				if ((cx > 0) & (cx <= 13))
				{
					g_bConfigDisasmOpcodesView ^= true;
					DebugDisplay();
				}
		}
		// Click on PC inside reg window?
		if ((cx >= 51) && (cx <= 60))
		{
			if (cy == 3)
			{
				CmdCursorJumpPC(CURSOR_ALIGN_CENTER);
				DebugDisplay();
			}
			else
				if (cy == 4 || cy == 5)
				{
					int iFlag = -1;
					int nFlag = _6502_NUM_FLAGS;

					while (nFlag-- > 0)
					{
						// TODO: magic number instead of DrawFlags() DISPLAY_FLAG_COLUMN, rect.left += ((2 + _6502_NUM_FLAGS) * nSpacerWidth);
						// BP_SRC_FLAG_C is 6,  cx 60 --> 0
						// ...
						// BP_SRC_FLAG_N is 13, cx 53 --> 7
						if (cx == (53 + nFlag))
						{
							iFlag = 7 - nFlag;
							break;
						}
					}

					if (iFlag >= 0)
					{
						regs.ps ^= (1 << iFlag);
						DebugDisplay();
					}
				}
				else // Click on stack
					if (cy > 3)
					{
					}
		}
	}
}

//===========================================================================
Update_t CmdConfigSetFont(int nArgs)
{
#if OLD_FONT
	HFONT  hFont = (HFONT)0;
	TCHAR* pFontName = NULL;
	int    nHeight = g_nFontHeight;
	int    iFontTarget = FONT_DISASM_DEFAULT;
	int    iFontPitch = FIXED_PITCH | FF_MODERN;
	//	int    iFontMode   =
	bool   bHaveTarget = false;
	bool   bHaveFont = false;

	if (!nArgs)
	{ // reset to defaut font
		pFontName = g_sFontNameDefault;
	}
	else
		if (nArgs <= 3)
		{
			int iArg = 1;
			pFontName = g_aArgs[1].sArg;

			// [DISASM|INFO|CONSOLE] "FontName" [#]
			// "FontName" can be either arg 1 or 2

			int iFound;
			int nFound = FindParam(g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_WINDOW_BEGIN, _PARAM_WINDOW_END);
			if (nFound)
			{
				switch (iFound)
				{
				case PARAM_DISASM: iFontTarget = FONT_DISASM_DEFAULT; iFontPitch = FIXED_PITCH | FF_MODERN; bHaveTarget = true; break;
				case PARAM_INFO: iFontTarget = FONT_INFO; iFontPitch = FIXED_PITCH | FF_MODERN; bHaveTarget = true; break;
				case PARAM_CONSOLE: iFontTarget = FONT_CONSOLE; iFontPitch = DEFAULT_PITCH | FF_DECORATIVE; bHaveTarget = true; break;
				default:
					if (g_aArgs[2].bType != TOKEN_QUOTE_DOUBLE)
						return Help_Arg_1(CMD_CONFIG_FONT);
					break;
				}
				if (bHaveTarget)
				{
					pFontName = g_aArgs[2].sArg;
				}
			}
			else
				if (nArgs == 2)
				{
					nHeight = atoi(g_aArgs[2].sArg);
					if ((nHeight < 6) || (nHeight > 36))
						nHeight = g_nFontHeight;
				}
		}
		else
		{
			return Help_Arg_1(CMD_CONFIG_FONT);
		}

	if (!_CmdConfigFont(iFontTarget, pFontName, iFontPitch, nHeight))
	{
	}
#endif
	return UPDATE_ALL;
}

void Util_CopyTextToClipboard(const size_t nSize, const char* pText)
{
	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nSize + 1);
	memcpy(GlobalLock(hMem), pText, nSize + 1);
	GlobalUnlock(hMem);

	OpenClipboard(0);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hMem);
	CloseClipboard();

	// GlobalFree() ??
}

void ProcessClipboardCommands()
{
	// Support Clipboard (paste)
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	if (!OpenClipboard(GetFrame().g_hFrameWindow))
		return;

	HGLOBAL hClipboard;
	LPTSTR  pData;

	hClipboard = GetClipboardData(CF_TEXT);
	if (hClipboard != NULL)
	{
		pData = (char*)GlobalLock(hClipboard);
		if (pData != NULL)
		{
			LPTSTR pSrc = pData;
			char c;

			while (true)
			{
				c = *pSrc++;

				if (!c)
					break;

				if (c == CHAR_CR)
				{
					// Eat char
				}
				else
					if (c == CHAR_LF)
					{
						DebuggerProcessCommand(true);
					}
					else
					{
						// If we didn't want verbatim, we could do:
						// DebuggerInputConsoleChar( c );
						if ((c >= CHAR_SPACE) && (c <= 126)) // HACK MAGIC # 32 -> ' ', # 126
							ConsoleInputChar(c);
					}
			}
			GlobalUnlock(hClipboard);
		}
	}
	CloseClipboard();

	UpdateDisplay(UPDATE_CONSOLE_DISPLAY);
}
