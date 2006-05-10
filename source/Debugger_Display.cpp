/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

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
 * Author: Copyright (C) 2006, Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop


#include <assert.h>


// Public _________________________________________________________________________________________

	const int DISPLAY_HEIGHT = 384; // 368; // DISPLAY_LINES * g_nFontHeight;

	FontConfig_t g_aFontConfig[ NUM_FONTS  ];

// Private ________________________________________________________________________________________

// Disassembly
	/*
		* Wingdings
			\xE1   Up Arrow
			\xE2   Down Arrow
		* Webdings // M$ Font
			\x35   Up Arrow
			\x33   Left Arrow  (\x71 recycl is too small to make out details)
			\x36   Down Arrow
		* Symols
			\xAD   Up Arrow
			\xAF   Down Arrow
		* ???
			\x18 Up
			\x19 Down
	*/
	TCHAR g_sConfigBranchIndicatorUp   [ NUM_DISASM_BRANCH_TYPES+1 ] = TEXT(" ^\x35");
	TCHAR g_sConfigBranchIndicatorEqual[ NUM_DISASM_BRANCH_TYPES+1 ] = TEXT(" =\x33");
	TCHAR g_sConfigBranchIndicatorDown [ NUM_DISASM_BRANCH_TYPES+1 ] = TEXT(" v\x36");

// Drawing
	// Width
		const int DISPLAY_WIDTH  = 560;
		#define  SCREENSPLIT1    356	// Horizontal Column (pixels?) of Stack & Regs
//		#define  SCREENSPLIT2    456	// Horizontal Column (pixels?) of BPs, Watches & Mem
		const int SCREENSPLIT2 = 456-7; // moved left one "char" to show PC in breakpoint:
		
		const int DISPLAY_BP_COLUMN      = SCREENSPLIT2;
		const int DISPLAY_MINI_CONSOLE   = SCREENSPLIT1 -  6; // - 1 chars
		const int DISPLAY_DISASM_RIGHT   = SCREENSPLIT1 -  6; // - 1 char
		const int DISPLAY_FLAG_COLUMN    = SCREENSPLIT1 + 63;
		const int DISPLAY_MINIMEM_COLUMN = SCREENSPLIT2 +  7;
		const int DISPLAY_REGS_COLUMN    = SCREENSPLIT1;
		const int DISPLAY_STACK_COLUMN   = SCREENSPLIT1;
		const int DISPLAY_TARGETS_COLUMN = SCREENSPLIT1;
		const int DISPLAY_WATCHES_COLUMN = SCREENSPLIT2;
		const int DISPLAY_ZEROPAGE_COLUMN= SCREENSPLIT1;

		const int MAX_DISPLAY_STACK_LINES    =  8;

	// Height
//		const int DISPLAY_LINES  =  24; // FIXME: Should be pixels
		// 304 = bottom of disassembly
		// 368 = bottom console
		// 384 = 16 * 24 very bottom
		const int DEFAULT_HEIGHT = 16;

	static HDC g_hDC = 0;


static	void SetupColorsHiLoBits ( HDC dc, bool bHiBit, bool bLoBit, 
			const int iBackground, const int iForeground,
			const int iColorHiBG , /*const int iColorHiFG,
			const int iColorLoBG , */const int iColorLoFG );
static	char ColorizeSpecialChar( HDC hDC, TCHAR * sText, BYTE nData, const MemoryView_e iView,
			const int iTxtBackground  = BG_INFO     , const int iTxtForeground  = FG_DISASM_CHAR,
			const int iHighBackground = BG_INFO_CHAR, const int iHighForeground = FG_INFO_CHAR_HI,
			const int iLowBackground  = BG_INFO_CHAR, const int iLowForeground  = FG_INFO_CHAR_LO );

	char  FormatCharTxtAsci ( const BYTE b, bool * pWasAsci_ );
	int FormatDisassemblyLine(  WORD nOffset, int iOpcode, int iOpmode, int nOpbytes,
		char *sAddress_, char *sOpCodes_,
		char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char *sTargetValue_,
		char * sImmediate_, char & nImmediate_, char *sBranch_ );



	void DrawSubWindow_Code ( int iWindow );
	void DrawSubWindow_IO       (Update_t bUpdate);
	void DrawSubWindow_Source1  (Update_t bUpdate);
	void DrawSubWindow_Source2  (Update_t bUpdate);
	void DrawSubWindow_Symbols  (Update_t bUpdate);
	void DrawSubWindow_ZeroPage (Update_t bUpdate);

	void DrawWindowBottom ( Update_t bUpdate, int iWindow );

// Utility ________________________________________________________________________________________


//===========================================================================
bool CanDrawDebugger()
{
	if (g_bDebuggerViewingAppleOutput)
		return false;

	if ((mode == MODE_DEBUG) || (mode == MODE_STEPPING))
		return true;

	return false;
}


//===========================================================================
int DebugDrawText ( LPCTSTR pText, RECT & rRect )
{
	int nLen = _tcslen( pText );
	ExtTextOut( g_hDC,
		rRect.left, rRect.top,
		ETO_CLIPPED | ETO_OPAQUE, &rRect,
		pText, nLen,
		NULL );
	return nLen;
}


// Also moves cursor 'non proportional' font width, using FONT_INFO
//===========================================================================
int DebugDrawTextFixed ( LPCTSTR pText, RECT & rRect )
{
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	int nChars = DebugDrawText( pText, rRect );
	rRect.left += (nFontWidth * nChars);
	return nChars;
}


//===========================================================================
int DebugDrawTextLine ( LPCTSTR pText, RECT & rRect )
{
	int nChars = DebugDrawText( pText, rRect );
	rRect.top    += g_nFontHeight;
	rRect.bottom += g_nFontHeight;
	return nChars;
}


// Moves cursor 'proportional' font width
//===========================================================================
int DebugDrawTextHorz ( LPCTSTR pText, RECT & rRect )
{
	int nFontWidth = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;

	SIZE size;
	int nChars = DebugDrawText( pText, rRect );
	if (GetTextExtentPoint32( g_hDC, pText, nChars,  &size ))
	{
		rRect.left += size.cx;
	}
	else
	{
		rRect.left += (nFontWidth * nChars);
	}
	return nChars;
}



//===========================================================================
char  FormatCharTxtAsci ( const BYTE b, bool * pWasAsci_ )
{
	if (pWasAsci_)
		*pWasAsci_ = false;

	char c = (b & 0x7F);
	if (b <= 0x7F)
	{
		if (pWasAsci_)
		{
			*pWasAsci_ = true;			
		}
	}
	return c;
}

//===========================================================================
char  FormatCharTxtCtrl ( const BYTE b, bool * pWasCtrl_ )
{
	if (pWasCtrl_)
		*pWasCtrl_ = false;

	char c = (b & 0x7F); // .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
	if (b < 0x20) // SPACE
	{
		if (pWasCtrl_)
		{
			*pWasCtrl_ = true;			
		}
		c = b + '@'; // map ctrl chars to visible
	}
	return c;
}

//===========================================================================
char  FormatCharTxtHigh ( const BYTE b, bool *pWasHi_ )
{
	if (pWasHi_)
		*pWasHi_ = false;

	char c = b;
	if (b > 0x7F)
	{
		if (pWasHi_)
		{
			*pWasHi_ = true;			
		}
		c = (b & 0x7F);
	}
	return c;
}


//===========================================================================
char FormatChar4Font ( const BYTE b, bool *pWasHi_, bool *pWasLo_ )
{
	// Most Windows Fonts don't have (printable) glyphs for control chars
	BYTE b1 = FormatCharTxtHigh( b , pWasHi_ );
	BYTE b2 = FormatCharTxtCtrl( b1, pWasLo_ );
	return b2;
}


// Disassembly formatting flags returned
//	@parama sTargetValue_ indirect/indexed final value
//===========================================================================
int FormatDisassemblyLine( WORD nBaseAddress, int iOpcode, int iOpmode, int nOpBytes,
	char *sAddress_, char *sOpCodes_,
	char *sTarget_, char *sTargetOffset_, int & nTargetOffset_,
	char *sTargetPointer_, char *sTargetValue_,
	char *sImmediate_, char & nImmediate_, char *sBranch_ )
{
	const int nMaxOpcodes = 3;
	unsigned int nMinBytesLen = (nMaxOpcodes * (2 + g_bConfigDisasmOpcodeSpaces)); // 2 char for byte (or 3 with space)

	int bDisasmFormatFlags = 0;

	// Composite string that has the symbol or target nAddress
	WORD nTarget = 0;
	nTargetOffset_ = 0;

//	if (g_aOpmodes[ iMode ]._sFormat[0])
	if ((iOpmode != AM_IMPLIED) &&
		(iOpmode != AM_1) &&
		(iOpmode != AM_2) &&
		(iOpmode != AM_3))
	{
		nTarget = *(LPWORD)(mem+nBaseAddress+1);
		if (nOpBytes == 2)
			nTarget &= 0xFF;

		if (iOpmode == AM_R) // Relative ADDR_REL)
		{
			nTarget = nBaseAddress+2+(int)(signed char)nTarget;

			// Always show branch indicators
			//	if ((nBaseAddress == regs.pc) && CheckJump(nAddress))
			bDisasmFormatFlags |= DISASM_BRANCH_INDICATOR;

			if (nTarget < nBaseAddress)
			{
				wsprintf( sBranch_, TEXT(" %c"), g_sConfigBranchIndicatorUp[ g_iConfigDisasmBranchType ] );
			}
			else
			if (nTarget > nBaseAddress)
			{
				wsprintf( sBranch_, TEXT(" %c"), g_sConfigBranchIndicatorDown[ g_iConfigDisasmBranchType ] );
			}
			else
			{
				wsprintf( sBranch_, TEXT("%c "), g_sConfigBranchIndicatorEqual[ g_iConfigDisasmBranchType ] );
			}
		}

//		if (_tcsstr(g_aOpmodes[ iOpmode ]._sFormat,TEXT("%s")))
//		if ((iOpmode >= ADDR_ABS) && (iOpmode <= ADDR_IABS))
		if ((iOpmode == AM_A  ) || // Absolute
			(iOpmode == AM_Z  ) || // Zeropage
			(iOpmode == AM_AX ) || // Absolute, X
			(iOpmode == AM_AY ) || // Absolute, Y
			(iOpmode == AM_ZX ) || // Zeropage, X
			(iOpmode == AM_ZY ) || // Zeropage, Y
			(iOpmode == AM_R  ) || // Relative
			(iOpmode == AM_IZX) || // Indexed (Zeropage Indirect, X)
			(iOpmode == AM_IAX) || // Indexed (Absolute Indirect, X)
			(iOpmode == AM_NZY) || // Indirect (Zeropage) Index, Y
			(iOpmode == AM_NZ ) || // Indirect (Zeropage)
			(iOpmode == AM_NA ))   // Indirect Absolute
		{
			LPCTSTR pTarget = NULL;
			LPCTSTR pSymbol = FindSymbolFromAddress( nTarget );
			if (pSymbol)
			{
				bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
				pTarget = pSymbol;
			}

			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget - 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
					bDisasmFormatFlags |= DISASM_TARGET_OFFSET;
					pTarget = pSymbol;
					nTargetOffset_ = +1; // U FA82   LDA #3F1 BREAK+1
				}
			}
			
			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget + 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
					bDisasmFormatFlags |= DISASM_TARGET_OFFSET;
					pTarget = pSymbol;
					nTargetOffset_ = -1; // U FA82 LDA #3F3 BREAK-1
				}
			}

			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
					pTarget = FormatAddress( nTarget, nOpBytes );
			}				

//			wsprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, pTarget );
			if (bDisasmFormatFlags & DISASM_TARGET_OFFSET)
			{
				int nAbsTargetOffset =  (nTargetOffset_ > 0) ? nTargetOffset_ : -nTargetOffset_;
				wsprintf( sTargetOffset_, "%d", nAbsTargetOffset );
			}
			wsprintf( sTarget_, "%s", pTarget );


		// Indirect / Indexed
			int nTargetPartial;
			int nTargetPointer;
			WORD nTargetValue = 0; // de-ref
			int nTargetBytes;
			Get6502Targets( nBaseAddress, &nTargetPartial, &nTargetPointer, &nTargetBytes );

			if (nTargetPointer != NO_6502_TARGET)
			{
				bDisasmFormatFlags |= DISASM_TARGET_POINTER;

				nTargetValue = *(LPWORD)(mem+nTargetPointer);

//				if (((iOpmode >= AM_A) && (iOpmode <= AM_NZ)) && (iOpmode != AM_R))
				// nTargetBytes refers to size of pointer, not size of value
//					wsprintf( sTargetValue_, "%04X", nTargetValue ); // & 0xFFFF

				wsprintf( sTargetPointer_, "%04X", nTargetPointer & 0xFFFF );

				if (iOpmode != AM_NA ) // Indirect Absolute
				{
					bDisasmFormatFlags |= DISASM_TARGET_VALUE;

					wsprintf( sTargetValue_, "%02X", nTargetValue & 0xFF );

					bDisasmFormatFlags |= DISASM_IMMEDIATE_CHAR;
					nImmediate_ = (BYTE) nTargetValue;
					wsprintf( sImmediate_, "%c", FormatCharTxtCtrl( FormatCharTxtHigh( nImmediate_, NULL ), NULL ) );
				}
				
//				if (iOpmode == AM_NA ) // Indirect Absolute
//					wsprintf( sTargetValue_, "%04X", nTargetPointer & 0xFFFF );
//				else
// //					wsprintf( sTargetValue_, "%02X", nTargetValue & 0xFF );
//					wsprintf( sTargetValue_, "%04X:%02X", nTargetPointer & 0xFFFF, nTargetValue & 0xFF );
			}
		}
		else
		if (iOpmode == AM_M)
		{
//			wsprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, (unsigned)nTarget );
			wsprintf( sTarget_, "%02X", (unsigned)nTarget );

			if (iOpmode == AM_M)
			{
				bDisasmFormatFlags |= DISASM_IMMEDIATE_CHAR;
				nImmediate_ = (BYTE) nTarget;
				wsprintf( sImmediate_, "%c", FormatCharTxtCtrl( FormatCharTxtHigh( nImmediate_, NULL ), NULL ) );
			}
		}
	}

	wsprintf( sAddress_, "%04X", nBaseAddress );

	// Opcode Bytes
	TCHAR *pDst = sOpCodes_;
	for( int iBytes = 0; iBytes < nOpBytes; iBytes++ )
	{
		BYTE nMem = (unsigned)*(mem+nBaseAddress+iBytes);
		wsprintf( pDst, TEXT("%02X"), nMem ); // sBytes+_tcslen(sBytes)
		pDst += 2;

		if (g_bConfigDisasmOpcodeSpaces)
		{
			_tcscat( pDst, TEXT(" " ) );
		}
		pDst++;
	}
    while (_tcslen(sOpCodes_) < nMinBytesLen)
	{
		_tcscat( sOpCodes_, TEXT(" ") );
	}

	return bDisasmFormatFlags;
}	


//===========================================================================
void SetupColorsHiLoBits ( HDC hDC, bool bHighBit, bool bCtrlBit,
	const int iBackground, const int iForeground,
	const int iColorHiBG , const int iColorHiFG,
	const int iColorLoBG , const int iColorLoFG )
{
	// 4 cases: 
	// Hi Lo Background Foreground -> just map Lo -> FG, Hi -> BG
	// 0  0  normal     normal     BG_INFO        FG_DISASM_CHAR   (dark cyan bright cyan)
	// 0  1  normal     LoFG       BG_INFO        FG_DISASM_OPCODE (dark cyan yellow)
	// 1  0  HiBG       normal     BG_INFO_CHAR   FG_DISASM_CHAR   (mid cyan  bright cyan)
	// 1  1  HiBG       LoFG       BG_INFO_CHAR   FG_DISASM_OPCODE (mid cyan  yellow)

	SetBkColor(   hDC, DebuggerGetColor( iBackground ));
	SetTextColor( hDC, DebuggerGetColor( iForeground ));

	if (bHighBit)
	{
		SetBkColor(   hDC, DebuggerGetColor( iColorHiBG ));
		SetTextColor( hDC, DebuggerGetColor( iColorHiFG )); // was iForeground
	}

	if (bCtrlBit)
	{
		SetBkColor(   hDC, DebuggerGetColor( iColorLoBG ));
		SetTextColor( hDC, DebuggerGetColor( iColorLoFG ));
	}
}


// To flush out color bugs... swap: iAsciBackground & iHighBackground
//===========================================================================
char ColorizeSpecialChar( HDC hDC, TCHAR * sText, BYTE nData, const MemoryView_e iView,
		const int iAsciBackground /*= 0           */, const int iTextForeground /*= FG_DISASM_CHAR */,
		const int iHighBackground /*= BG_INFO_CHAR*/, const int iHighForeground /*= FG_INFO_CHAR_HI*/,
		const int iCtrlBackground /*= BG_INFO_CHAR*/, const int iCtrlForeground /*= FG_INFO_CHAR_LO*/ )
{
	bool bHighBit = false;
	bool bAsciBit = false;
	bool bCtrlBit = false;

	int iTextBG = iAsciBackground;
	int iHighBG = iHighBackground;
	int iCtrlBG = iCtrlBackground;
	int iTextFG = iTextForeground;
	int iHighFG = iHighForeground;
	int iCtrlFG = iCtrlForeground;

	BYTE nByte = FormatCharTxtHigh( nData, & bHighBit );
	char nChar = FormatCharTxtCtrl( nByte, & bCtrlBit );

	switch (iView)
	{
		case MEM_VIEW_ASCII:
			iHighBG = iTextBG;
			iCtrlBG = iTextBG;
			break;
		case MEM_VIEW_APPLE:
			iHighBG = iTextBG;
			if (!bHighBit)
			{
				iTextBG = iCtrlBG;
			}
						
			if (bCtrlBit)
			{
				iTextFG = iCtrlFG;
				if (bHighBit)
				{
					iHighFG = iTextFG;
				}
			}
			bCtrlBit = false;
			break;
		default: break;
	}

	if (sText)
		wsprintf( sText, TEXT("%c"), nChar );

	if (hDC)
	{
		SetupColorsHiLoBits( hDC, bHighBit, bCtrlBit
			, iTextBG, iTextFG // FG_DISASM_CHAR   
			, iHighBG, iHighFG // BG_INFO_CHAR     
			, iCtrlBG, iCtrlFG // FG_DISASM_OPCODE 
		);
	}
	return nChar;
}


// Main Windows ___________________________________________________________________________________


//===========================================================================
void DrawBreakpoints (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_BP_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	const int MAX_BP_LEN = 16;
	TCHAR sText[16] = TEXT("Breakpoints"); // TODO: Move to BP1

	SetBkColor(dc, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
	DebugDrawText( sText, rect );
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;

	int iBreakpoint;
	for (iBreakpoint = 0; iBreakpoint < NUM_BREAKPOINTS; iBreakpoint++ )
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
		WORD nLength   = pBP->nLength;

#if DEBUG_FORCE_DISPLAY
		nLength = 2;
#endif
		if (nLength)
		{
			bool bSet      = pBP->bSet;
			bool bEnabled  = pBP->bEnabled;
			WORD nAddress1 = pBP->nAddress;
			WORD nAddress2 = nAddress1 + nLength - 1;
			
			RECT rect2;
			rect2 = rect;
			
			SetBkColor( dc, DebuggerGetColor( BG_INFO ));
			SetTextColor( dc, DebuggerGetColor( FG_INFO_BULLET ) );
			wsprintf( sText, TEXT("%d"), iBreakpoint+1 );
			DebugDrawTextFixed( sText, rect2 );

			SetTextColor( dc, DebuggerGetColor( FG_INFO_OPERATOR ) );
			_tcscpy( sText, TEXT(":") );
			DebugDrawTextFixed( sText, rect2 );

#if DEBUG_FORCE_DISPLAY
	pBP->eSource = (BreakpointSource_t) iBreakpoint;
#endif
			SetTextColor( dc, DebuggerGetColor( FG_INFO_REG ) );
			int nRegLen = DebugDrawTextFixed( g_aBreakpointSource[ pBP->eSource ], rect2 );

			// Pad to 2 chars
			if (nRegLen < 2) // (g_aBreakpointSource[ pBP->eSource ][1] == 0) // HACK: Avoid strlen()
				rect2.left += g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

			SetBkColor( dc, DebuggerGetColor( BG_INFO ));
			SetTextColor( dc, DebuggerGetColor( FG_INFO_BULLET ) );
#if DEBUG_FORCE_DISPLAY
	if (iBreakpoint < 3)
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint * 2);
	else
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint-3 + BP_OP_READ);
#endif
			DebugDrawTextFixed( g_aBreakpointSymbols [ pBP->eOperator ], rect2 );

			DebugColors_e iForeground;
			DebugColors_e iBackground = BG_INFO;

			if (bSet)
			{
				if (bEnabled)
				{
					iBackground = BG_DISASM_BP_S_C;
//					iForeground = FG_DISASM_BP_S_X;
					iForeground = FG_DISASM_BP_S_C;
				}
				else
				{
					iForeground = FG_DISASM_BP_0_X;
				}
			}
			else
			{
				iForeground = FG_INFO_TITLE;
			}

			SetBkColor( dc, DebuggerGetColor( iBackground ) );
			SetTextColor( dc, DebuggerGetColor( iForeground ) );

#if DEBUG_FORCE_DISPLAY
	int iColor = R8 + (iBreakpoint*2);
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 4)
	{
		SetBkColor  ( dc, DebuggerGetColor( BG_DISASM_BP_S_C ) );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_C );
	}
	SetTextColor( dc, nColor );
#endif

			wsprintf( sText, TEXT("%04X"), nAddress1 );
			DebugDrawTextFixed( sText, rect2 );

			if (nLength > 1)
			{
				SetBkColor( dc, DebuggerGetColor( BG_INFO ) );
				SetTextColor( dc, DebuggerGetColor( FG_INFO_OPERATOR ) );

//				if (g_bConfigDisasmOpcodeSpaces)
//				{
//					DebugDrawTextHorz( TEXT(" "), rect2 );
//					rect2.left += g_nFontWidthAvg;
//				}

				DebugDrawTextFixed( TEXT("-"), rect2 );
//				rect2.left += g_nFontWidthAvg;
//				if (g_bConfigDisasmOpcodeSpaces) // TODO: Might have to remove spaces, for BPIO... addr-addr xx
//				{
//					rect2.left += g_nFontWidthAvg;
//				}

				SetBkColor( dc, DebuggerGetColor( iBackground ) );
				SetTextColor( dc, DebuggerGetColor( iForeground ) );
#if DEBUG_FORCE_DISPLAY
	iColor++;
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 4)
	{
		nColor = DebuggerGetColor( BG_INFO );
		SetBkColor( dc, nColor );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_X );
	}
	SetTextColor( dc, nColor );
#endif
				wsprintf( sText, TEXT("%04X"), nAddress2 );
				DebugDrawTextFixed( sText, rect2 );
			}

			// Bugfix: Rest of line is still breakpoint background color
			SetBkColor(dc, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
			SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
			DebugDrawTextHorz( TEXT(" "), rect2 );

		}
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}


//===========================================================================
void DrawConsoleInput( HDC dc )
{
	g_hDC = dc;

	SetTextColor( g_hDC, DebuggerGetColor( FG_CONSOLE_INPUT ));
	SetBkColor(   g_hDC, DebuggerGetColor( BG_CONSOLE_INPUT ));

	DrawConsoleLine( g_aConsoleInput, 0 );
}



//===========================================================================
int GetConsoleLineHeightPixels()
{
	int nHeight = nHeight = g_aFontConfig[ FONT_CONSOLE ]._nFontHeight; // _nLineHeight; // _nFontHeight;

	if (g_iFontSpacing == FONT_SPACING_CLASSIC)
	{
		nHeight++;  // "Classic" Height/Spacing
	}
	else
	if (g_iFontSpacing == FONT_SPACING_CLEAN)
	{
		nHeight++;
	}
	else
	if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
	{
		// default case handled
	}
	
	return nHeight;
}


//===========================================================================
int GetConsoleTopPixels( int y )
{
	int nLineHeight = GetConsoleLineHeightPixels();
	int nTop = DISPLAY_HEIGHT - ((y + 1) * nLineHeight);
	return nTop;
}


//===========================================================================
void DrawConsoleLine( LPCSTR pText, int y )
{
	if (y < 0)
		return;

//	int nHeight = WindowGetHeight( g_iWindowThis );
	int nLineHeight = GetConsoleLineHeightPixels();

	RECT rect;
	rect.left   = 0;
//	rect.top    = (g_nTotalLines - y) * nFontHeight; // DISPLAY_HEIGHT - (y * nFontHeight); // ((g_nTotalLines - y) * g_nFontHeight; // 368 = 23 lines * 16 pixels/line // MAX_DISPLAY_CONSOLE_LINES

	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;

	// Default: (356-14) = 342 pixels ~= 48 chars (7 pixel width)
	//	rect.right = SCREENSPLIT1-14;
//	rect.right = (g_nConsoleDisplayWidth * g_nFontWidthAvg) + (g_nFontWidthAvg - 1);

	int nMiniConsoleRight = DISPLAY_MINI_CONSOLE; // (g_nConsoleDisplayWidth * g_nFontWidthAvg) + (g_nFontWidthAvg * 2); // +14
	int nFullConsoleRight = DISPLAY_WIDTH;
	int nRight = g_bConsoleFullWidth ? nFullConsoleRight : nMiniConsoleRight;
	rect.right = nRight;

	DebugDrawText( pText, rect );
}


//===========================================================================
WORD DrawDisassemblyLine (HDC dc, int iLine, WORD nBaseAddress, LPTSTR text)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return 0;

	const int nMaxAddressLen = 40;
	const int nMaxOpcodes = 3;

	int iOpcode;
	int iOpmode;
	int nOpbytes;
	iOpcode = _6502GetOpmodeOpbytes( nBaseAddress, iOpmode, nOpbytes );

	const int CHARS_FOR_ADDRESS = 8; // 4 digits plus null

	TCHAR sAddress  [ CHARS_FOR_ADDRESS ];
	TCHAR sOpcodes  [(nMaxOpcodes*3)+1] = TEXT("");
	TCHAR sTarget   [nMaxAddressLen] = TEXT("");

	TCHAR sTargetOffset[ CHARS_FOR_ADDRESS ] = TEXT(""); // +/- 255, realistically +/-1
	int   nTargetOffset;

	TCHAR sTargetPointer[ CHARS_FOR_ADDRESS ] = TEXT("");
	TCHAR sTargetValue  [ CHARS_FOR_ADDRESS ] = TEXT("");

	char  nImmediate = 0;
	TCHAR sImmediate[ 4 ]; // 'c'
	TCHAR sBranch   [ 4 ]; // ^

	bool bTargetIndirect = false;
	bool bTargetX     = false;
	bool bTargetY     = false; // need to dislay ",Y"
	bool bTargetValue = false;

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NA))
		bTargetIndirect = true;

	if (((iOpmode >= AM_A) && (iOpmode <= AM_ZY)) || bTargetIndirect)
		bTargetValue = true;

	if ((iOpmode == AM_AX) || (iOpmode == AM_ZX) || (iOpmode == AM_IZX) || (iOpmode == AM_IAX))
		bTargetX = true;

	if ((iOpmode == AM_AY) || (iOpmode == AM_ZY)) // Note: AM_NZY is checked and triggered by bTargetIndirect
		bTargetY = true;

	int bDisasmFormatFlags = FormatDisassemblyLine( nBaseAddress, iOpcode, iOpmode, nOpbytes,
		sAddress, sOpcodes, sTarget, sTargetOffset, nTargetOffset, sTargetPointer, sTargetValue, sImmediate, nImmediate, sBranch );

	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	//> xxxx: xx xx xx   LABEL    MNEMONIC    'E' =
	//>       ^          ^        ^           ^   ^
	//>       6          17       27          41  46
	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	int X_OPCODE      =  6 * nDefaultFontWidth;
	int X_LABEL       = 17 * nDefaultFontWidth;
	int X_INSTRUCTION = 26 * nDefaultFontWidth; // 27
	int X_IMMEDIATE   = 40 * nDefaultFontWidth; // 41
	int X_BRANCH      = 46 * nDefaultFontWidth;

	const int DISASM_SYMBOL_LEN = 9;

	if (dc)
	{
		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight; // _nFontHeight; // g_nFontHeight

		RECT linerect;
		linerect.left   = 0;
		linerect.top    = iLine * nFontHeight;
		linerect.right  = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 14 -> g_nFontWidthAvg
		linerect.bottom = linerect.top + nFontHeight;

//		BOOL bp = g_nBreakpoints && CheckBreakpoint(nBaseAddress,nBaseAddress == regs.pc);

		bool bBreakpointActive;
		bool bBreakpointEnable;
		GetBreakpointInfo( nBaseAddress, bBreakpointActive, bBreakpointEnable );
		bool bAddressAtPC = (nBaseAddress == regs.pc);

		DebugColors_e iBackground = BG_DISASM_1;
		DebugColors_e iForeground = FG_DISASM_MNEMONIC; // FG_DISASM_TEXT;
		bool bCursorLine = false;

		if (((! g_bDisasmCurBad) && (iLine == g_nDisasmCurLine))
			|| (g_bDisasmCurBad && (iLine == 0)))
		{
			bCursorLine = true;

			// Breakpoint,
			if (bBreakpointActive)
			{
				if (bBreakpointEnable)
				{
					iBackground = BG_DISASM_BP_S_C; iForeground = FG_DISASM_BP_S_C; 
				}
				else
				{
					iBackground = BG_DISASM_BP_0_C; iForeground = FG_DISASM_BP_0_C;
				}
			}
			else
			if (bAddressAtPC)
			{
				iBackground = BG_DISASM_PC_C; iForeground = FG_DISASM_PC_C;
			}
			else
			{
				iBackground = BG_DISASM_C; iForeground = FG_DISASM_C;
				// HACK?  Sync Cursor back up to Address
				// The cursor line may of had to be been moved, due to Disasm Singularity.
				g_nDisasmCurAddress = nBaseAddress; 
			}
		}
		else
		{
			if (iLine & 1)
			{
				iBackground = BG_DISASM_1;
			}
			else
			{
				iBackground = BG_DISASM_2;
			}

			// This address has a breakpoint, but the cursor is not on it (atm)
			if (bBreakpointActive)
			{
				if (bBreakpointEnable) 
				{
					iForeground = FG_DISASM_BP_S_X; // Red (old Yellow)
				}
				else
				{
					iForeground = FG_DISASM_BP_0_X; // Yellow
				}				
			}
			else
			if (bAddressAtPC)
			{
				iBackground = BG_DISASM_PC_X; iForeground = FG_DISASM_PC_X;
			}
			else
			{
				iForeground = FG_DISASM_MNEMONIC;
			}
		}
		SetBkColor(   dc, DebuggerGetColor( iBackground ) );
		SetTextColor( dc, DebuggerGetColor( iForeground ) );

	// Address
		if (! bCursorLine)
			SetTextColor( dc, DebuggerGetColor( FG_DISASM_ADDRESS ) );
		DebugDrawTextHorz( (LPCTSTR) sAddress, linerect );

	// Address Seperator		
		if (! bCursorLine)
			SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			DebugDrawTextHorz( TEXT(":"), linerect );

	// Opcodes
		linerect.left = X_OPCODE;

		if (! bCursorLine)
			SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPCODE ) );
//		DebugDrawTextHorz( TEXT(" "), linerect );
		DebugDrawTextHorz( (LPCTSTR) sOpcodes, linerect );
//		DebugDrawTextHorz( TEXT("  "), linerect );

	// Label
		linerect.left = X_LABEL;

		LPCSTR pSymbol = FindSymbolFromAddress( nBaseAddress );
		if (pSymbol)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_SYMBOL ) );
			DebugDrawTextHorz( pSymbol, linerect );
		}	
//		linerect.left += (g_nFontWidthAvg * DISASM_SYMBOL_LEN);
//		DebugDrawTextHorz( TEXT(" "), linerect );

	// Instruction
		linerect.left = X_INSTRUCTION;

		if (! bCursorLine)
			SetTextColor( dc, DebuggerGetColor( iForeground ) );

		LPCTSTR pMnemonic = g_aOpcodes[ iOpcode ].sMnemonic;
		DebugDrawTextHorz( pMnemonic, linerect );

		DebugDrawTextHorz( TEXT(" "), linerect );

	// Target
		if (iOpmode == AM_M)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("#$"), linerect );
		}

		if (bTargetIndirect)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("("), linerect );
		}

		char *pTarget = sTarget;
		if (*pTarget == '$')
		{
			pTarget++;
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("$"), linerect );
		}

		if (! bCursorLine)
		{
			if (bDisasmFormatFlags & DISASM_TARGET_SYMBOL)
			{
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_SYMBOL ) );
			}
			else
			{
				if (iOpmode == AM_M)
//				if (bDisasmFormatFlags & DISASM_IMMEDIATE_CHAR)
				{
					SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPCODE ) );
				}
				else	
				{
					SetTextColor( dc, DebuggerGetColor( FG_DISASM_TARGET ) );
				}
			}
		}
		DebugDrawTextHorz( pTarget, linerect );
//		DebugDrawTextHorz( TEXT(" "), linerect );

		// Target Offset +/-		
		if (bDisasmFormatFlags & DISASM_TARGET_OFFSET)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));	

			if (nTargetOffset > 0)
				DebugDrawTextHorz( TEXT("+" ), linerect );
			if (nTargetOffset < 0)
				DebugDrawTextHorz( TEXT("-" ), linerect );

			if (! bCursorLine)
			{
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPCODE )); // Technically, not a hex number, but decimal
			}		
			DebugDrawTextHorz( sTargetOffset, linerect );
		}
		// Indirect Target Regs
		if (bTargetIndirect || bTargetX || bTargetY)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));	

			if (bTargetX)
				DebugDrawTextHorz( TEXT(",X"), linerect );

			if (bTargetY)
				DebugDrawTextHorz( TEXT(",Y"), linerect );

			if (bTargetIndirect)
				DebugDrawTextHorz( TEXT(")"), linerect );

			if (iOpmode == AM_NZY)
				DebugDrawTextHorz( TEXT(",Y"), linerect );
		}

	// Immediate Char
		linerect.left = X_IMMEDIATE;

	// Memory Pointer and Value
		if (bDisasmFormatFlags & DISASM_TARGET_POINTER) // (bTargetValue)
		{
//			DebugDrawTextHorz( TEXT("  "), linerect );

			// FG_DISASM_TARGET
			// FG_DISASM_OPERATOR
			// FG_DISASM_OPCODE
			if (! bCursorLine)
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_ADDRESS ));

			DebugDrawTextHorz( sTargetPointer, linerect );

			if (bDisasmFormatFlags & DISASM_TARGET_VALUE)
			{
				if (! bCursorLine)
					SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ));
				DebugDrawTextHorz( TEXT(":"), linerect );

				if (! bCursorLine)
					SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPCODE ));

				DebugDrawTextHorz( sTargetValue, linerect );
				DebugDrawTextHorz( TEXT(" "), linerect );
			}
		}

		if (bDisasmFormatFlags & DISASM_IMMEDIATE_CHAR)
		{
			if (! bCursorLine)
			{
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ) );
			}

		if (! (bDisasmFormatFlags & DISASM_TARGET_POINTER))
			DebugDrawTextHorz( TEXT("'"), linerect ); // TEXT("    '")

			if (! bCursorLine)
			{
				ColorizeSpecialChar( dc, NULL, nImmediate, MEM_VIEW_ASCII, iBackground );
//					iBackground, FG_INFO_CHAR_HI, FG_DISASM_CHAR, FG_INFO_CHAR_LO );
			}
			DebugDrawTextHorz( sImmediate, linerect );

			SetBkColor(   dc, DebuggerGetColor( iBackground ) ); // Hack: Colorize can "color bleed to EOL"
			if (! bCursorLine)
			{
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_OPERATOR ) );
			}

		if (! (bDisasmFormatFlags & DISASM_TARGET_POINTER))
			DebugDrawTextHorz( TEXT("'"), linerect );
		}
//		else
//		if (bTargetIndirect)
//		{
//			// Follow indirect targets
//		}
	
	// Branch Indicator		
		linerect.left = X_BRANCH;

		if (bDisasmFormatFlags & DISASM_BRANCH_INDICATOR)
		{
			if (! bCursorLine)
			{
				SetTextColor( dc, DebuggerGetColor( FG_DISASM_BRANCH ) );
			}

			if (g_iConfigDisasmBranchType == DISASM_BRANCH_FANCY)
				SelectObject( dc, g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont );  // g_hFontWebDings

			DebugDrawText( sBranch, linerect );

			if (g_iConfigDisasmBranchType)
				SelectObject( dc, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm
		}
	}

	return nOpbytes;
}


// Optionally copy the flags to pText_
//===========================================================================
void DrawFlags (HDC dc, int line, WORD nRegFlags, LPTSTR pFlagNames_)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	TCHAR sFlagNames[ _6502_NUM_FLAGS+1 ] = TEXT(""); // = TEXT("NVRBDIZC"); // copy from g_aFlagNames
	TCHAR sText[2] = TEXT("?");
	RECT  rect;

	if (dc)
	{
		rect.left   = DISPLAY_FLAG_COLUMN;
		rect.top    = line * g_nFontHeight;
		rect.right  = rect.left + 9;
		rect.bottom = rect.top + g_nFontHeight;
		SetBkColor(dc, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	}

	int iFlag = 0;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		iFlag = BP_SRC_FLAG_C + (_6502_NUM_FLAGS - nFlag - 1);

		bool bSet = (nRegFlags & 1);
		if (dc)
		{
//			sText[0] = g_aFlagNames[ MAX_FLAGS - iFlag - 1]; // mnemonic[iFlag]; // mnemonic is reversed
			sText[0] = g_aBreakpointSource[ iFlag ][0];
			if (bSet)
			{
				SetBkColor( dc, DebuggerGetColor( BG_INFO_INVERSE ));
				SetTextColor( dc, DebuggerGetColor( FG_INFO_INVERSE ));
			}
			else
			{
				SetBkColor(dc, DebuggerGetColor( BG_INFO ));
				SetTextColor( dc, DebuggerGetColor( FG_INFO_TITLE ));
			}
			DebugDrawText( sText, rect );
			rect.left  -= 9; // HACK: Font Width
			rect.right -= 9; // HACK: Font Width
		}

		if (pFlagNames_)
		{
			if (! bSet) //(nFlags & 1))
			{
				sFlagNames[nFlag] = TEXT('.');
			}
			else
			{
				sFlagNames[nFlag] = g_aBreakpointSource[ iFlag ][0]; 
			}
		}

		nRegFlags >>= 1;
	}

	if (pFlagNames_)
		_tcscpy(pFlagNames_,sFlagNames);
}

//===========================================================================
void DrawMemory (HDC hDC, int line, int iMemDump )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];

	USHORT       nAddr   = pMD->nAddress;
	DEVICE_e     eDevice = pMD->eDevice;
	MemoryView_e iView   = pMD->eView;

	SS_CARD_MOCKINGBOARD SS_MB;

	if ((eDevice == DEV_SY6522) || (eDevice == DEV_AY8910))
		MB_GetSnapshot(&SS_MB, 4+(nAddr>>1));		// Slot4 or Slot5

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.left   = DISPLAY_MINIMEM_COLUMN - nFontWidth;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;


	const int MAX_MEM_VIEW_TXT = 16;
	TCHAR sText[ MAX_MEM_VIEW_TXT * 2 ];
	TCHAR sData[ MAX_MEM_VIEW_TXT * 2 ];

	TCHAR sType   [ 4 ] = TEXT("Mem");
	TCHAR sAddress[ 8 ] = TEXT("");

	int iForeground = FG_INFO_OPCODE;
	int iBackground = BG_INFO;

	if (eDevice == DEV_SY6522)
	{
//		wsprintf(sData,TEXT("Mem at SY#%d"), nAddr);
		wsprintf( sAddress,TEXT("SY#%d"), nAddr );
	}
	else if(eDevice == DEV_AY8910)
	{
//		wsprintf(sData,TEXT("Mem at AY#%d"), nAddr);
		wsprintf( sAddress,TEXT("AY#%d"), nAddr );
	}
	else
	{
		wsprintf( sAddress,TEXT("%04X"),(unsigned)nAddr);

		if (iView == MEM_VIEW_HEX)
			wsprintf( sType, TEXT("HEX") );
		else
		if (iView == MEM_VIEW_ASCII)
			wsprintf( sType, TEXT("ASCII") );
		else
			wsprintf( sType, TEXT("TEXT") );
	}

	RECT rect2;

	rect2 = rect;	
	SetTextColor( hDC, DebuggerGetColor( FG_INFO_TITLE ));
	SetBkColor( hDC, DebuggerGetColor( BG_INFO ));
	DebugDrawTextFixed( sType, rect2 );

	SetTextColor( hDC, DebuggerGetColor( FG_INFO_OPERATOR ));
	DebugDrawTextFixed( TEXT(" at " ), rect2 );

	SetTextColor( hDC, DebuggerGetColor( FG_INFO_ADDRESS ));
	DebugDrawTextLine( sAddress, rect2 );

	rect.top    = rect2.top;
	rect.bottom = rect2.bottom;

	sData[0] = 0;

	WORD iAddress = nAddr;

	if( (eDevice == DEV_SY6522) || (eDevice == DEV_AY8910) )
	{
		iAddress = 0;
	}

	int nLines = 4;
	int nCols = 4;

	if (iView != MEM_VIEW_HEX)
	{
		nCols = MAX_MEM_VIEW_TXT;
	}
	rect.right = DISPLAY_WIDTH;

	SetTextColor( hDC, DebuggerGetColor( FG_INFO_OPCODE ));

	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		RECT rect2;
		rect2 = rect;

		if (iView == MEM_VIEW_HEX)
		{
			wsprintf( sText, TEXT("%04X"), iAddress );
			SetTextColor( hDC, DebuggerGetColor( FG_INFO_ADDRESS ));
			DebugDrawTextFixed( sText, rect2 );

			SetTextColor( hDC, DebuggerGetColor( FG_INFO_OPERATOR ));
			DebugDrawTextFixed( TEXT(":"), rect2 );
		}

		for (int iCol = 0; iCol < nCols; iCol++)
		{
			bool bHiBit = false;
			bool bLoBit = false;

			SetBkColor  ( hDC, DebuggerGetColor( iBackground ));
			SetTextColor( hDC, DebuggerGetColor( iForeground ));

// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
//			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
//			{
//				wsprintf( sText, TEXT("IO ") );
//			}
//			else
			if (eDevice == DEV_SY6522)
			{
				wsprintf( sText, TEXT("%02X "), (unsigned) ((BYTE*)&SS_MB.Unit[nAddr & 1].RegsSY6522)[iAddress] );
			}
			else
			if (eDevice == DEV_AY8910)
			{
				wsprintf( sText, TEXT("%02X "), (unsigned)SS_MB.Unit[nAddr & 1].RegsAY8910[iAddress] );
			}
			else
			{
				BYTE nData = (unsigned)*(LPBYTE)(mem+iAddress);
				sText[0] = 0;

				char c = nData;

				if (iView == MEM_VIEW_HEX)
				{
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
					{
						SetTextColor( hDC, DebuggerGetColor( FG_INFO_IO_BYTE ));
					}
					wsprintf(sText, TEXT("%02X "), nData );
				}
				else
				{
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
						iBackground = BG_INFO_IO_BYTE;

					ColorizeSpecialChar( hDC, sText, nData, iView, iBackground );
				}
			}
			int nChars = DebugDrawTextFixed( sText, rect2 ); // DebugDrawTextFixed()

			iAddress++;
		}
		rect.top    += g_nFontHeight; // TODO/FIXME: g_nFontHeight;
		rect.bottom += g_nFontHeight; // TODO/FIXME: g_nFontHeight;
		sData[0] = 0;
	}
}

//===========================================================================
void DrawRegister (HDC dc, int line, LPCTSTR name, const int nBytes, const WORD nValue, int iSource )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_REGS_COLUMN;
	rect.top    = line * g_nFontHeight;
	rect.right  = rect.left + 40; // TODO:FIXME: g_nFontWidthAvg * 
	rect.bottom = rect.top + g_nFontHeight;

	if ((PARAM_REG_A  == iSource) ||
		(PARAM_REG_X  == iSource) ||
		(PARAM_REG_Y  == iSource) ||
		(PARAM_REG_PC == iSource) ||
		(PARAM_REG_SP == iSource))
	{		
		SetTextColor(dc, DebuggerGetColor( FG_INFO_REG ));
	}
	else
	{
		SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE ));
	}
	SetBkColor(dc, DebuggerGetColor( BG_INFO ));
	DebugDrawText( name, rect );

	unsigned int nData = nValue;
	int nOffset = 6;

	TCHAR sValue[8];

	if (PARAM_REG_SP == iSource)
	{
		WORD nStackDepth = _6502_STACK_END - nValue;
		wsprintf( sValue, "%02X", nStackDepth );
		int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
		rect.left += (2 * nFontWidth) + (nFontWidth >> 1); // 2.5 looks a tad nicer then 2

		// ## = Stack Depth (in bytes)
		SetTextColor(dc, DebuggerGetColor( FG_INFO_OPERATOR )); // FG_INFO_OPCODE, FG_INFO_TITLE
		DebugDrawText( sValue, rect );
	}

	if (nBytes == 2)
	{
		wsprintf(sValue,TEXT("%04X"), nData);
	}
	else
	{
		rect.left  = DISPLAY_REGS_COLUMN + 21; // HACK: MAGIC #: 21 // +3 chars
		rect.right = SCREENSPLIT2;

		SetTextColor(dc, DebuggerGetColor( FG_INFO_OPERATOR ));
		DebugDrawTextFixed( TEXT("'"), rect ); // DebugDrawTextFixed

		ColorizeSpecialChar( dc, sValue, nData, MEM_VIEW_ASCII ); // MEM_VIEW_APPLE for inverse background little hard on the eyes
		DebugDrawTextFixed( sValue, rect ); // DebugDrawTextFixed()

		SetBkColor(dc, DebuggerGetColor( BG_INFO ));
		SetTextColor(dc, DebuggerGetColor( FG_INFO_OPERATOR ));
		DebugDrawTextFixed( TEXT("'"), rect ); // DebugDrawTextFixed()

		wsprintf(sValue,TEXT("  %02X"), nData );
	}

	// Needs to be far enough over, since 4 chars of ZeroPage symbol also calls us
	rect.left  = DISPLAY_REGS_COLUMN + (nOffset * g_aFontConfig[ FONT_INFO ]._nFontWidthAvg); // HACK: MAGIC #: 40 
	rect.right = SCREENSPLIT2;

	if ((PARAM_REG_PC == iSource) || (PARAM_REG_SP == iSource)) // Stack Pointer is target address, but doesn't look as good.
	{
		SetTextColor(dc, DebuggerGetColor( FG_INFO_ADDRESS ));
	}
	else
	{
		SetTextColor(dc, DebuggerGetColor( FG_INFO_OPCODE )); // FG_DISASM_OPCODE
	}
	DebugDrawText( sValue, rect );
}


//===========================================================================
void DrawSourceLine( int iSourceLine, RECT &rect )
{
	TCHAR sLine[ CONSOLE_WIDTH ];

	ZeroMemory( sLine, CONSOLE_WIDTH );

	if ((iSourceLine >=0) && (iSourceLine < g_AssemblerSourceBuffer.GetNumLines() ))
	{
		char * pSource = g_AssemblerSourceBuffer.GetLine( iSourceLine );

//		int nLenSrc = _tcslen( pSource );
//		if (nLenSrc >= CONSOLE_WIDTH)
//			bool bStop = true;

		TextConvertTabsToSpaces( sLine, pSource, CONSOLE_WIDTH-1 ); // bugfix 2,3,1,15: fence-post error, buffer over-run

//		int nLenTab = _tcslen( sLine );
	}
	else
	{
		_tcscpy( sLine, TEXT(" "));
	}

	DebugDrawText( sLine, rect );
	rect.top += g_nFontHeight;
//	iSourceLine++;
}


//===========================================================================
void DrawStack (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	unsigned nAddress = regs.sp;
#if DEBUG_FORCE_DISPLAY
	nAddress = 0x100;
#endif

	int      iStack   = 0;
	while (iStack < MAX_DISPLAY_STACK_LINES)
	{
		nAddress++;

		RECT rect;
		rect.left   = DISPLAY_STACK_COLUMN;
		rect.top    = (iStack+line) * g_nFontHeight;
		rect.right  = DISPLAY_STACK_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.bottom = rect.top + g_nFontHeight;

		SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE )); // [COLOR_STATIC
		SetBkColor(dc, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA

		TCHAR sText[8] = TEXT("");
		if (nAddress <= _6502_STACK_END)
		{
			wsprintf(sText,TEXT("%04X"),nAddress);
		}

		DebugDrawText( sText, rect );

		rect.left   = DISPLAY_STACK_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.right  = SCREENSPLIT2;
		SetTextColor(dc, DebuggerGetColor( FG_INFO_OPCODE )); // COLOR_FG_DATA_TEXT

		if (nAddress <= _6502_STACK_END)
		{
			wsprintf(sText,TEXT("%02X"),(unsigned)*(LPBYTE)(mem+nAddress));
		}
		DebugDrawText( sText, rect );
	    iStack++;
	}
}


//===========================================================================
void DrawTargets (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int aTarget[2];
	Get6502Targets( regs.pc, &aTarget[0],&aTarget[1], NULL );

	RECT rect;
	
	int iAddress = 2;
	while (iAddress--)
	{
		// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
//		if ((aTarget[iAddress] >= _6502_IO_BEGIN) && (aTarget[iAddress] <= _6502_IO_END))
//			aTarget[iAddress] = NO_6502_TARGET;

		TCHAR sAddress[8] = TEXT("");
		TCHAR sData[8]   = TEXT("");

#if DEBUG_FORCE_DISPLAY
		if (aTarget[iAddress] == NO_6502_TARGET)
			aTarget[iAddress] = 0;
#endif
		if (aTarget[iAddress] != NO_6502_TARGET)
		{
			wsprintf(sAddress,TEXT("%04X"),aTarget[iAddress]);
			if (iAddress)
				wsprintf(sData,TEXT("%02X"),*(LPBYTE)(mem+aTarget[iAddress]));
			else
				wsprintf(sData,TEXT("%04X"),*(LPWORD)(mem+aTarget[iAddress]));
		}

		rect.left   = DISPLAY_TARGETS_COLUMN;
		rect.top    = (line+iAddress) * g_nFontHeight;
		int nColumn = DISPLAY_TARGETS_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.right  = nColumn;
		rect.bottom = rect.top + g_nFontHeight;

		if (iAddress == 0)
			SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE )); // Temp Address
		else
			SetTextColor(dc, DebuggerGetColor( FG_INFO_ADDRESS )); // Target Address

		SetBkColor(dc, DebuggerGetColor( BG_INFO ));
		DebugDrawText( sAddress, rect );

		rect.left  = nColumn; // SCREENSPLIT1+40; // + 40
		rect.right = SCREENSPLIT2;

		if (iAddress == 0)
			SetTextColor(dc, DebuggerGetColor( FG_INFO_ADDRESS )); // Temp Target
		else
			SetTextColor(dc, DebuggerGetColor( FG_INFO_OPCODE )); // Target Bytes

		DebugDrawText( sData, rect );
  }
}

//===========================================================================
void DrawWatches (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_WATCHES_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	TCHAR sText[16] = TEXT("Watches");
	SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE ));
	SetBkColor(dc, DebuggerGetColor( BG_INFO ));
	DebugDrawTextLine( sText, rect );

	int iWatch;
	for (iWatch = 0; iWatch < MAX_WATCHES; iWatch++ )
	{
#if DEBUG_FORCE_DISPLAY
		if (true)
#else
		if (g_aWatches[iWatch].bEnabled)
#endif
		{
			RECT rect2 = rect;

			wsprintf( sText,TEXT("%d"),iWatch+1  );
			SetTextColor( dc, DebuggerGetColor( FG_INFO_BULLET ));
			DebugDrawTextFixed( sText, rect2 );
			
			wsprintf( sText,TEXT(":") );
			SetTextColor( dc, DebuggerGetColor( FG_INFO_OPERATOR ));
			DebugDrawTextFixed( sText, rect2 );

			wsprintf( sText,TEXT(" %04X"), g_aWatches[iWatch].nAddress );
			SetTextColor( dc, DebuggerGetColor( FG_INFO_ADDRESS ));
			DebugDrawTextFixed( sText, rect2 );

			wsprintf(sText,TEXT(" %02X"),(unsigned)*(LPBYTE)(mem+g_aWatches[iWatch].nAddress));
			SetTextColor(dc, DebuggerGetColor( FG_INFO_OPCODE ));
			DebugDrawTextFixed( sText, rect2 );
		}

		rect.top    += g_nFontHeight; // HACK: 
		rect.bottom += g_nFontHeight; // HACK:
	}
}


//===========================================================================
void DrawZeroPagePointers(HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	for(int iZP = 0; iZP < MAX_ZEROPAGE_POINTERS; iZP++)
	{
		RECT rect;
		rect.left   = DISPLAY_ZEROPAGE_COLUMN;
		rect.top    = (line+iZP) * g_nFontHeight;
		rect.right  = SCREENSPLIT2; // TODO/FIXME:
		rect.bottom = rect.top + g_nFontHeight;

		TCHAR sText[8] = TEXT("       ");

		SetTextColor(dc, DebuggerGetColor( FG_INFO_TITLE )); // COLOR_STATIC
		SetBkColor(dc, DebuggerGetColor( BG_INFO ));
		DebugDrawText( sText, rect );

		Breakpoint_t *pZP = &g_aZeroPagePointers[iZP];
		bool bEnabled = pZP->bEnabled;

#if DEBUG_FORCE_DISPLAY
		bEnabled = true;
#endif

		if (bEnabled)
//		if (g_aZeroPagePointers[iZP].bSet) // TODO: Only list enanbled ones
		{
			const UINT nSymbolLen = 4;
			char szZP[nSymbolLen+1];

			BYTE nZPAddr1 = (g_aZeroPagePointers[iZP].nAddress  ) & 0xFF; // +MJP missig: "& 0xFF", or "(BYTE) ..."
			BYTE nZPAddr2 = (g_aZeroPagePointers[iZP].nAddress+1) & 0xFF;

			// Get nZPAddr1 last (for when neither symbol is not found - GetSymbol() return ptr to static buffer)
			const char* pszSymbol2 = GetSymbol(nZPAddr2, 2);		// 2:8-bit value (if symbol not found)
			const char* pszSymbol1 = GetSymbol(nZPAddr1, 2);		// 2:8-bit value (if symbol not found)

			if( (strlen(pszSymbol1)==1) && (strlen(pszSymbol2)==1) )
			{
				sprintf(szZP, "%s%s", pszSymbol1, pszSymbol2);
			}
			else
			{
				memcpy(szZP, pszSymbol1, nSymbolLen);
				szZP[nSymbolLen] = 0;
			}

			WORD nZPPtr = (WORD)mem[ nZPAddr1 ] | ((WORD)mem[ nZPAddr2 ]<< 8);
			DrawRegister(dc, line+iZP, szZP, 2, nZPPtr);
		}
	}
}


// Sub Windows ____________________________________________________________________________________


//===========================================================================
void DrawSubWindow_Console (Update_t bUpdate)
{
	if (! CanDrawDebugger())
		return;

	SelectObject( g_hDC, g_aFontConfig[ FONT_CONSOLE ]._hFont );

//	static TCHAR sConsoleBlank[ CONSOLE_WIDTH ];
	
	if ((bUpdate & UPDATE_CONSOLE_INPUT) || (bUpdate & UPDATE_CONSOLE_DISPLAY))
	{
		SetTextColor( g_hDC, DebuggerGetColor( FG_CONSOLE_OUTPUT )); // COLOR_FG_CONSOLE
		SetBkColor(   g_hDC, DebuggerGetColor( BG_CONSOLE_OUTPUT )); // COLOR_BG_CONSOLE

//		int nLines = MIN(g_nConsoleDisplayTotal - g_iConsoleDisplayStart, g_nConsoleDisplayHeight);
		int iLine = g_iConsoleDisplayStart + CONSOLE_FIRST_LINE;
		for (int y = 0; y < g_nConsoleDisplayHeight ; y++ )
		{
			if (iLine <= (g_nConsoleDisplayTotal + CONSOLE_FIRST_LINE))
			{
				DrawConsoleLine( g_aConsoleDisplay[ iLine ], y+1 );
			}
			iLine++;
//			else
//				DrawConsoleLine( sConsoleBlank, y );
		}

		DrawConsoleInput( g_hDC );
	}
}	

//===========================================================================
void DrawSubWindow_Data (Update_t bUpdate)
{
	HDC hDC = g_hDC;
	int iBackground;	

	const int nMaxOpcodes = WINDOW_DATA_BYTES_PER_LINE;
	TCHAR sAddress  [ 5];

	assert( CONSOLE_WIDTH > WINDOW_DATA_BYTES_PER_LINE );

	TCHAR sOpcodes  [ CONSOLE_WIDTH ] = TEXT("");
	TCHAR sImmediate[ 4 ]; // 'c'

	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	int X_OPCODE      =  6                    * nDefaultFontWidth;
	int X_CHAR        = (6 + (nMaxOpcodes*3)) * nDefaultFontWidth;

	int iMemDump = 0;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	USHORT       nAddress = pMD->nAddress;
	DEVICE_e     eDevice  = pMD->eDevice;
	MemoryView_e iView    = pMD->eView;

	if (!pMD->bActive)
		return;

	int  iByte;
	WORD iAddress = nAddress;

	int iLine;
	int nLines = g_nDisasmWinHeight;

	for (iLine = 0; iLine < nLines; iLine++ )
	{
		iAddress = nAddress;

	// Format
		wsprintf( sAddress, TEXT("%04X"), iAddress );

		sOpcodes[0] = 0;
		for ( iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nData = (unsigned)*(LPBYTE)(mem + iAddress + iByte);
			wsprintf( &sOpcodes[ iByte * 3 ], TEXT("%02X "), nData );
		}
		sOpcodes[ nMaxOpcodes * 3 ] = 0;

		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight;

	// Draw
		RECT rect;
		rect.left   = 0;
		rect.top    = iLine * nFontHeight;
		rect.right  = DISPLAY_DISASM_RIGHT;
		rect.bottom = rect.top + nFontHeight;

		if (iLine & 1)
		{
			iBackground = BG_DATA_1;
		}
		else
		{
			iBackground = BG_DATA_2;
		}
		SetBkColor( hDC, DebuggerGetColor( iBackground ) );

		SetTextColor( hDC, DebuggerGetColor( FG_DISASM_ADDRESS ) );
		DebugDrawTextHorz( (LPCTSTR) sAddress, rect );

		SetTextColor( hDC, DebuggerGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			DebugDrawTextHorz( TEXT(":"), rect );

		rect.left = X_OPCODE;

		SetTextColor( hDC, DebuggerGetColor( FG_DATA_BYTE ) );
		DebugDrawTextHorz( (LPCTSTR) sOpcodes, rect );

		rect.left = X_CHAR;

	// Seperator
		SetTextColor( hDC, DebuggerGetColor( FG_DISASM_OPERATOR ));
		DebugDrawTextHorz( (LPCSTR) TEXT("  |  " ), rect );


	// Plain Text
		SetTextColor( hDC, DebuggerGetColor( FG_DISASM_CHAR ) );

		MemoryView_e eView = pMD->eView;
		if ((eView != MEM_VIEW_ASCII) && (eView != MEM_VIEW_APPLE))
			eView = MEM_VIEW_ASCII;

		iAddress = nAddress;
		for (iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(mem + iAddress);
			int iTextBackground = iBackground;
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
			{
				iTextBackground = BG_INFO_IO_BYTE;
			}

			ColorizeSpecialChar( hDC, sImmediate, (BYTE) nImmediate, eView, iBackground );
			DebugDrawTextHorz( (LPCSTR) sImmediate, rect );

			iAddress++;
		}
/*
	// Colorized Text
		iAddress = nAddress;
		for (iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(membank + iAddress);
			int iTextBackground = iBackground; // BG_INFO_CHAR;
//pMD->eView == MEM_VIEW_HEX
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
				iTextBackground = BG_INFO_IO_BYTE;

			ColorizeSpecialChar( hDC, sImmediate, (BYTE) nImmediate, MEM_VIEW_APPLE, iBackground );
			DebugDrawTextHorz( (LPCSTR) sImmediate, rect );

			iAddress++;
		}

		SetBkColor( hDC, DebuggerGetColor( iBackground ) ); // Hack, colorize Char background "spills over to EOL"
		DebugDrawTextHorz( (LPCSTR) TEXT(" " ), rect );
*/
		SetBkColor( hDC, DebuggerGetColor( iBackground ) ); // HACK: Colorize() can "spill over" to EOL

		SetTextColor( hDC, DebuggerGetColor( FG_DISASM_OPERATOR ));
		DebugDrawTextHorz( (LPCSTR) TEXT("  |  " ), rect );

		nAddress += nMaxOpcodes;
	}
}


// DrawRegisters();
//===========================================================================
void DrawSubWindow_Info( int iWindow )
{
	if (g_iWindowThis == WINDOW_CONSOLE)
		return;

	const TCHAR **sReg = g_aBreakpointSource;

	DrawStack(g_hDC,0);
	DrawTargets(g_hDC,9);
	DrawRegister(g_hDC,12, sReg[ BP_SRC_REG_A ] , 1, regs.a , PARAM_REG_A  );
	DrawRegister(g_hDC,13, sReg[ BP_SRC_REG_X ] , 1, regs.x , PARAM_REG_X  );
	DrawRegister(g_hDC,14, sReg[ BP_SRC_REG_Y ] , 1, regs.y , PARAM_REG_Y  );
	DrawRegister(g_hDC,15, sReg[ BP_SRC_REG_PC] , 2, regs.pc, PARAM_REG_PC );
	DrawRegister(g_hDC,16, sReg[ BP_SRC_REG_S ] , 2, regs.sp, PARAM_REG_SP );
	DrawFlags(g_hDC,17,regs.ps,NULL);
	DrawZeroPagePointers(g_hDC,19);

#if defined(SUPPORT_Z80_EMU) && defined(OUTPUT_Z80_REGS)
	DrawRegister(g_hDC,19,TEXT("AF"),2,*(WORD*)(membank+REG_AF));
	DrawRegister(g_hDC,20,TEXT("BC"),2,*(WORD*)(membank+REG_BC));
	DrawRegister(g_hDC,21,TEXT("DE"),2,*(WORD*)(membank+REG_DE));
	DrawRegister(g_hDC,22,TEXT("HL"),2,*(WORD*)(membank+REG_HL));
	DrawRegister(g_hDC,23,TEXT("IX"),2,*(WORD*)(membank+REG_IX));
#endif

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nBreakpoints)
#endif
		DrawBreakpoints(g_hDC,0);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nWatches)
#endif
		DrawWatches(g_hDC,7);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[0].bActive)
#endif
		DrawMemory(g_hDC, 14, 0 ); // g_aMemDump[0].nAddress, g_aMemDump[0].eDevice);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[1].bActive)
#endif
		DrawMemory(g_hDC, 19, 1 ); // g_aMemDump[1].nAddress, g_aMemDump[1].eDevice);

}

//===========================================================================
void DrawSubWindow_IO (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_Source (Update_t bUpdate)
{
}


//===========================================================================
void DrawSubWindow_Source2 (Update_t bUpdate)
{
//	if (! g_bSourceLevelDebugging)
//		return;

	SetTextColor( g_hDC, DebuggerGetColor( FG_SOURCE ));

	int iSource = g_iSourceDisplayStart;
	int nLines  = g_nDisasmWinHeight;

	int y = g_nDisasmWinHeight;
	int nHeight = g_nDisasmWinHeight;

	if (g_aWindowConfig[ g_iWindowThis ].bSplit) // HACK: Split Window Height is odd, so bottom window gets +1 height
		nHeight++;

	RECT rect;
	rect.top    = (y * g_nFontHeight);
	rect.bottom = rect.top + (nHeight * g_nFontHeight);
	rect.left = 0;
	rect.right = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 7

// Draw Title
	TCHAR sTitle[ CONSOLE_WIDTH ];
	TCHAR sText [ CONSOLE_WIDTH ];
	_tcscpy( sTitle, TEXT("   Source: "));
	_tcsncpy( sText, g_aSourceFileName, g_nConsoleDisplayWidth - _tcslen( sTitle ) - 1 );
	_tcscat( sTitle, sText );

	SetBkColor(   g_hDC, DebuggerGetColor( BG_SOURCE_TITLE ));
	SetTextColor( g_hDC, DebuggerGetColor( FG_SOURCE_TITLE ));
	DebugDrawText( sTitle, rect );
	rect.top += g_nFontHeight;

// Draw Source Lines
	int iBackground;
	int iForeground;

	int iSourceCursor = 2; // (g_nDisasmWinHeight / 2);
	int iSourceLine = FindSourceLine( regs.pc );

	if (iSourceLine == NO_SOURCE_LINE)
	{
		iSourceCursor = NO_SOURCE_LINE;
	}
	else
	{
		iSourceLine -= iSourceCursor;
		if (iSourceLine < 0)
			iSourceLine = 0;
	}

	for( int iLine = 0; iLine < nLines; iLine++ )
	{
		if (iLine != iSourceCursor)
		{
			iBackground = BG_SOURCE_1;
			if (iLine & 1)
				iBackground = BG_SOURCE_2;
			iForeground = FG_SOURCE;
		}
		else
		{
			// Hilighted cursor line
			iBackground = BG_DISASM_PC_X; // _C
			iForeground = FG_DISASM_PC_X; // _C
		}
		SetBkColor(   g_hDC, DebuggerGetColor( iBackground ));
		SetTextColor( g_hDC, DebuggerGetColor( iForeground ));

		DrawSourceLine( iSourceLine, rect );
		iSourceLine++;
	}
}

//===========================================================================
void DrawSubWindow_Symbols (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_ZeroPage (Update_t bUpdate)
{
}


//===========================================================================
void DrawWindow_Code( Update_t bUpdate )
{
	DrawSubWindow_Code( g_iWindowThis );

//	DrawWindowTop( g_iWindowThis );
	DrawWindowBottom( bUpdate, g_iWindowThis );

	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Console( Update_t bUpdate )
{
	// Nothing to do, except draw background, since text handled by DrawSubWindow_Console()
    RECT viewportrect;
    viewportrect.left   = 0;
    viewportrect.top    = 0;
    viewportrect.right  = DISPLAY_WIDTH;
    viewportrect.bottom = DISPLAY_HEIGHT - DEFAULT_HEIGHT; // 368 = 23 lines // TODO/FIXME

// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !
    SetBkColor(g_hDC, DebuggerGetColor( BG_DISASM_2 )); // COLOR_BG_CODE
	// Can't use DebugDrawText, since we don't ned the CLIPPED flag
	// TODO: add default param OPAQUE|CLIPPED
    ExtTextOut( g_hDC
		,0,0
		,ETO_OPAQUE
		,&viewportrect
		,TEXT("")
		,0
		,NULL
	);
}

//===========================================================================
void DrawWindow_Data( Update_t bUpdate )
{
	DrawSubWindow_Data( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_IO( Update_t bUpdate )
{
	DrawSubWindow_IO( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Source( Update_t bUpdate )
{
	DrawSubWindow_Source( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Symbols( Update_t bUpdate )
{
	DrawSubWindow_Symbols( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

void DrawWindow_ZeroPage( Update_t bUpdate )
{
	DrawSubWindow_ZeroPage( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindowBackground_Main( int g_iWindowThis )
{
    RECT viewportrect;
    viewportrect.left   = 0;
    viewportrect.top    = 0;
    viewportrect.right  = SCREENSPLIT1 - 6; // HACK: MAGIC #: 14 -> 6 -> (g_nFonWidthAvg-1)
    viewportrect.bottom = DISPLAY_HEIGHT - DEFAULT_HEIGHT; // 368 = 23 lines // TODO/FIXME
// g_nFontHeight * g_nDisasmWinHeight; // 304

// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !

    SetBkColor(g_hDC, DebuggerGetColor( BG_DISASM_1 )); // COLOR_BG_CODE
	// Can't use DebugDrawText, since we don't need CLIPPED
    ExtTextOut(g_hDC,0,0,ETO_OPAQUE,&viewportrect,TEXT(""),0,NULL);
}

//===========================================================================
void DrawWindowBackground_Info( int g_iWindowThis )
{
    RECT viewportrect;
    viewportrect.top    = 0;
    viewportrect.left   = SCREENSPLIT1 - 6; // 14 // HACK: MAGIC #: 14 -> (g_nFontWidthAvg-1)
    viewportrect.right  = 560;
    viewportrect.bottom = DISPLAY_HEIGHT; //g_nFontHeight * MAX_DISPLAY_INFO_LINES; // 384

	SetBkColor(g_hDC, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	// Can't use DebugDrawText, since we don't need CLIPPED
	ExtTextOut(g_hDC,0,0,ETO_OPAQUE,&viewportrect,TEXT(""),0,NULL);
}


//===========================================================================
void UpdateDisplay (Update_t bUpdate)
{
	g_hDC = FrameGetDC();

	SelectObject( g_hDC, g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger

	SetTextAlign(g_hDC,TA_TOP | TA_LEFT);

	if ((bUpdate & UPDATE_BREAKPOINTS)
		|| (bUpdate & UPDATE_DISASM)
		|| (bUpdate & UPDATE_FLAGS)
		|| (bUpdate & UPDATE_MEM_DUMP)
		|| (bUpdate & UPDATE_REGS)
		|| (bUpdate & UPDATE_STACK)
		|| (bUpdate & UPDATE_SYMBOLS)
		|| (bUpdate & UPDATE_TARGETS)
		|| (bUpdate & UPDATE_WATCH)
		|| (bUpdate & UPDATE_ZERO_PAGE))
	{
		bUpdate |= UPDATE_BACKGROUND;
		bUpdate |= UPDATE_CONSOLE_INPUT;
	}
	
	if (bUpdate & UPDATE_BACKGROUND)
	{
		if (g_iWindowThis != WINDOW_CONSOLE)
		{
			DrawWindowBackground_Main( g_iWindowThis );
			DrawWindowBackground_Info( g_iWindowThis );
		}
	}
	
	switch( g_iWindowThis )
	{
		case WINDOW_CODE:
			DrawWindow_Code( bUpdate );
			break;

		case WINDOW_CONSOLE:
			DrawWindow_Console( bUpdate );
			break;

		case WINDOW_DATA:
			DrawWindow_Data( bUpdate );
			break;

		case WINDOW_IO:
			DrawWindow_IO( bUpdate );

		case WINDOW_SOURCE:
			DrawWindow_Source( bUpdate );

		case WINDOW_SYMBOLS:
			DrawWindow_Symbols( bUpdate );
			break;

		case WINDOW_ZEROPAGE:
			DrawWindow_ZeroPage( bUpdate );

		default:
			break;
	}

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY) || (bUpdate & UPDATE_CONSOLE_INPUT))
		DrawSubWindow_Console( bUpdate );

	FrameReleaseDC();
	g_hDC = 0;
}




//===========================================================================
void DrawWindowBottom ( Update_t bUpdate, int iWindow )
{
	if (! g_aWindowConfig[ iWindow ].bSplit)
		return;

	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

//	if (pWindow->eBot == WINDOW_DATA)
//	{
//		DrawWindow_Data( bUpdate, false );
//	}
	
	if (pWindow->eBot == WINDOW_SOURCE)
		DrawSubWindow_Source2( bUpdate );
}

//===========================================================================
void DrawSubWindow_Code ( int iWindow )
{
	int nLines = g_nDisasmWinHeight;

	// Check if we have a bad disasm
	// BUG: This still doesn't catch all cases
	// G FB53, SPACE, PgDn * 
	// Note: DrawDisassemblyLine() has kludge.
//		DisasmCalcTopFromCurAddress( false );
	// These should be functionally equivalent.
	//	DisasmCalcTopFromCurAddress();
	//	DisasmCalcBotFromTopAddress();
	SelectObject( g_hDC, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm 

	WORD nAddress = g_nDisasmTopAddress; // g_nDisasmCurAddress;
	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		nAddress += DrawDisassemblyLine( g_hDC, iLine, nAddress, NULL);
	}

	SelectObject( g_hDC, g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger
}
