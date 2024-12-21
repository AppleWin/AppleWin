/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski
Copyright (C) 2020, Tom Charlesworth, Michael Pohoreski, Cyril Lambin

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
 * Author: Copyright (C) 2006-2020 Michael Pohoreski, (C) 2020 Cyril Lambin
 */

#include "StdAfx.h"

#include "Debug.h"
#include "Debugger_Display.h"
#include "Debugger_Disassembler.h"

#include "../Core.h"
#include "../Interface.h"
#include "../CPU.h"
#include "../Windows/Win32Frame.h"
#include "../LanguageCard.h"
#include "../CardManager.h"
#include "../Memory.h"
#include "../Mockingboard.h"
#include "../NTSC.h"

// NEW UI debugging - force display ALL meta-info (regs, stack, bp, watches, zp) for debugging purposes
#define DEBUG_FORCE_DISPLAY 0

#define SOFTSWITCH_OLD 0
#define SOFTSWITCH_LANGCARD 1

#if _DEBUG
	#define DEBUG_FONT_NO_BACKGROUND_CHAR      0
	#define DEBUG_FONT_NO_BACKGROUND_TEXT      0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_CON  0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_INFO 0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_MAIN 0

	// no top console line
	#define DEBUG_BACKGROUND 0
#endif

	#define DISPLAY_MEMORY_TITLE     1
//	#define DISPLAY_BREAKPOINT_TITLE 1
//	#define DISPLAY_WATCH_TITLE      1


// Public _________________________________________________________________________________________

// Font
	FontConfig_t g_aFontConfig[ NUM_FONTS  ];

// Private ________________________________________________________________________________________

	char g_aDebuggerVirtualTextScreen[ DEBUG_VIRTUAL_TEXT_HEIGHT ][ DEBUG_VIRTUAL_TEXT_WIDTH ];

// HACK HACK HACK
	//g_nDisasmWinHeight
	WindowSplit_t *g_pDisplayWindow = 0; // HACK
// HACK

// Display - Win32
	static HDC g_hDebuggerMemDC = NULL;
	static HBITMAP g_hDebuggerMemBM = NULL;
	static LPBITMAPINFO  g_pDebuggerMemFramebufferinfo = NULL;
	static bgra_t* g_pDebuggerMemFramebits = NULL;

	static HDC g_hConsoleFontDC = NULL;
	static HBITMAP g_hConsoleFontBitmap = NULL;
	static LPBITMAPINFO  g_hConsoleFontFramebufferinfo = NULL;
	static bgra_t* g_hConsoleFontFramebits;

	char g_cConsoleBrushFG_r;
	char g_cConsoleBrushFG_g;
	char g_cConsoleBrushFG_b;
	char g_cConsoleBrushBG_r;
	char g_cConsoleBrushBG_g;
	char g_cConsoleBrushBG_b;

	static HBRUSH g_hConsoleBrushFG = NULL;
	static HBRUSH g_hConsoleBrushBG = NULL;

	// NOTE: Keep in sync ConsoleColors_e g_anConsoleColor !
	COLORREF g_anConsoleColor[ NUM_CONSOLE_COLORS ] =
	{                         // # <Bright Blue Green Red>
		RGB(   0,   0,   0 ), // 0 0000 K
		RGB( 255,  32,  32 ), // 1 1001 R
		RGB(   0, 255,   0 ), // 2 1010 G
		RGB( 255, 255,   0 ), // 3 1011 Y
		RGB(  64,  64, 255 ), // 4 1100 B
		RGB( 255,   0, 255 ), // 5 1101 M Purple/Magenta now used for warnings.
		RGB(   0, 255, 255 ), // 6 1110 C
		RGB( 255, 255, 255 ), // 7 1111 W
		RGB( 255, 128,   0 ), // 8 0011 Orange
		RGB( 128, 128, 128 ), // 9 0111 Grey

		RGB(  80, 192, 255 )  // Lite Blue
	};

// Drawing
	// Width
		const int DISPLAY_WIDTH  = 560;
		// New Font = 50.5 char * 7 px/char = 353.5
		const int DISPLAY_DISASM_RIGHT   = 353 ;

#if USE_APPLE_FONT
		// Horizontal Column (pixels) of Stack & Regs
		const int INFO_COL_1 = (51 * CONSOLE_FONT_WIDTH);
		const int DISPLAY_REGS_COLUMN       = INFO_COL_1;
		const int DISPLAY_FLAG_COLUMN       = INFO_COL_1;
		const int DISPLAY_STACK_COLUMN      = INFO_COL_1;
		const int DISPLAY_TARGETS_COLUMN    = INFO_COL_1;
		const int DISPLAY_ZEROPAGE_COLUMN   = INFO_COL_1;
		const int DISPLAY_SOFTSWITCH_COLUMN = INFO_COL_1 - (CONSOLE_FONT_WIDTH/2) + 1; // 1/2 char width padding around soft switches

		// Horizontal Column (pixels) of BPs, Watches
		const int INFO_COL_2 = (62 * 7); // nFontWidth
		const int DISPLAY_BP_COLUMN      = INFO_COL_2;
		const int DISPLAY_WATCHES_COLUMN = INFO_COL_2;

		// Horizontal Column (pixels) of VideoScannerInfo & Mem
		const int INFO_COL_3 = (63 * 7); // nFontWidth
		const int DISPLAY_MINIMEM_COLUMN = INFO_COL_3;
		const int DISPLAY_VIDEO_SCANNER_COLUMN = INFO_COL_3;
		const int DISPLAY_IRQ_COLUMN = INFO_COL_3 + (12 * 7); // (12 chars from v/h-pos) * nFontWidth
#else
		const int DISPLAY_CPU_INFO_LEFT_COLUMN = SCREENSPLIT1;	// TC: SCREENSPLIT1 is not defined anywhere in the .sln!

		const int DISPLAY_REGS_COLUMN       = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_FLAG_COLUMN       = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_STACK_COLUMN      = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_TARGETS_COLUMN    = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_ZEROPAGE_COLUMN   = DISPLAY_CPU_INFO_LEFT_COLUMN;
		const int DISPLAY_SOFTSWITCH_COLUMN = DISPLAY_CPU_INFO_LEFT_COLUMN - (CONSOLE_FONT_WIDTH/2);

		const int SCREENSPLIT2 = SCREENSPLIT1 + (12 * 7); // moved left 3 chars to show B. prefix in breakpoint #, W. prefix in watch #
		const int DISPLAY_BP_COLUMN      = SCREENSPLIT2;
		const int DISPLAY_WATCHES_COLUMN = SCREENSPLIT2;
		const int DISPLAY_MINIMEM_COLUMN = SCREENSPLIT2; // nFontWidth
		const int DISPLAY_VIDEO_SCANNER_COLUMN = SCREENSPLIT2;
#endif

		int MAX_DISPLAY_REGS_LINES        = 7;
		int MAX_DISPLAY_STACK_LINES       = 8;
		int MAX_DISPLAY_TARGET_PTR_LINES  = 2;
		int MAX_DISPLAY_ZEROPAGE_LINES    = 8;

//		int MAX_DISPLAY_BREAKPOINTS_LINES = 7; // 7
//		int MAX_DISPLAY_WATCHES_LINES     = 8; // 8
		int MAX_DISPLAY_MEMORY_LINES_1    = 4; // 4
		int MAX_DISPLAY_MEMORY_LINES_2    = 4; // 4 // 2
		int g_nDisplayMemoryLines;

	// Height
//		const int DISPLAY_LINES  =  24; // FIXME: Should be pixels
		// 304 = bottom of disassembly
		// 368 = bottom console
		// 384 = 16 * 24 very bottom
//		const int DEFAULT_HEIGHT = 16;

		VideoScannerDisplayInfo g_videoScannerDisplayInfo;

	char  FormatCharTxtAsci ( const BYTE b, bool * pWasAsci_ );

	void DrawSubWindow_Code ( int iWindow );
	void DrawSubWindow_IO       (Update_t bUpdate);
	void DrawSubWindow_Source   (Update_t bUpdate);
	void DrawSubWindow_Source2  (Update_t bUpdate);
	void DrawSubWindow_Symbols  (Update_t bUpdate);
	void DrawSubWindow_ZeroPage (Update_t bUpdate);


	void DrawWindowBottom ( Update_t bUpdate, int iWindow );

	void DrawRegister(int line, LPCTSTR name, int bytes, WORD value, int iSource = 0);


// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/pantdraw_6n77.asp
enum WinROP4_e
{
	DSna    = 0x00220326,
	DPSao   = 0x00EA02E9,
};

/*
	Reverse Polish Notation
		a 	Bitwise AND
		n 	Bitwise NOT (inverse)
		o 	Bitwise OR
		x 	Bitwise exclusive OR (XOR)

	Pen(P)         	1 	1 	0 	0 	Decimal Result
	Dest(D)        	1 	0 	1 	0     	Boolean Operation

	R2_BLACK       	0 	0 	0 	0 	0 	0
	R2_NOTMERGEPEN 	0 	0 	0 	1 	1 	~(P | D)
	R2_MASKNOTPEN  	0 	0 	1 	0 	2 	~P & D
	R2_NOTCOPYPEN 	0 	0 	1 	1 	3 	~P
	R2_MASKPENNOT 	0 	1 	0 	0 	4 	P & ~D
	R2_NOT         	0 	1 	0 	1 	5 	~D
	R2_XORPEN      	0 	1 	1 	0 	6 	P ^ D
	R2_NOTMASKPEN  	0 	1 	1 	1 	7 	~(P & D)
	R2_MASKPEN     	1 	0 	0 	0 	8 	P & D
	R2_NOTXORPEN   	1 	0 	0 	1 	9 	~(P ^ D)
	R2_NOPR2_NOP   	1 	0 	1 	0 	10 	D
	R2_MERGENOTPEN 	1 	0 	1 	1 	11 	~P | D
	R2_COPYPEN     	1 	1 	0 	0 	12 	P (default)
	R2_MERGEPENNOT 	1 	1 	0 	1 	13 	P | ~D
	R2_MERGEPEN    	1 	1 	1 	0 	14 	P | D
	R2_WHITE       	1 	1 	1 	1 	15 	1 
*/

#if DEBUG_FONT_ROP
const uint32_t aROP4[ 256 ] =
{
	0x00000042, // BLACKNESS
	0x00010289, // DPSoon 	
	0x00020C89, // DPSona 	
	0x000300AA, // PSon 	
	0x00040C88, // SDPona 	
	0x000500A9, // DPon 	
	0x00060865, // PDSxnon 	
	0x000702C5, // PDSaon 	
	0x00080F08, // SDPnaa 	
	0x00090245, // PDSxon 	
	0x000A0329, // DPna 	
	0x000B0B2A, // PSDnaon 	
	0x000C0324, // SPna 	
	0x000D0B25, // PDSnaon 	
	0x000E08A5, // PDSonon 	
	0x000F0001, // Pn 	
	0x00100C85, // PDSona 	
	0x001100A6, // DSon 	NOTSRCERASE
	0x00120868, // SDPxnon 	
	0x001302C8, // SDPaon 	
	0x00140869, // DPSxnon 	
	0x001502C9, // DPSaon 	
	0x00165CCA, // PSDPSanaxx 	// 16
	0x00171D54, // SSPxDSxaxn 	
	0x00180D59, // SPxPDxa 	
	0x00191CC8, // SDPSanaxn 	
	0x001A06C5, // PDSPaox 	
	0x001B0768, // SDPSxaxn 	
	0x001C06CA, // PSDPaox 	
	0x001D0766, // DSPDxaxn 	
	0x001E01A5, // PDSox 	
	0x001F0385, // PDSoan 	
	0x00200F09, // DPSnaa 	
	0x00210248, // SDPxon 	
	0x00220326, // DSna 	
	0x00230B24, // SPDnaon 	
	0x00240D55, // SPxDSxa 	
	0x00251CC5, // PDSPanaxn 	
	0x002606C8, // SDPSaox 	
	0x00271868, // SDPSxnox 	
	0x00280369, // DPSxa 	
	0x002916CA, // PSDPSaoxxn 	
	0x002A0CC9, // DPSana 	
	0x002B1D58, // SSPxPDxaxn 	
	0x002C0784, // SPDSoax 	
	0x002D060A, // PSDnox 	
	0x002E064A, // PSDPxox 	
	0x002F0E2A, // PSDnoan 	
	0x0030032A, // PSna 	
	0x00310B28, // SDPnaon 	
	0x00320688, // SDPSoox 	
	0x00330008, // Sn 	// 33 NOTSRCCOPY
	0x003406C4, // SPDSaox 	
	0x00351864, // SPDSxnox 	
	0x003601A8, // SDPox 	
	0x00370388, // SDPoan 	
	0x0038078A, // PSDPoax 	
	0x00390604, // SPDnox 	
	0x003A0644, // SPDSxox 	
	0x003B0E24, // SPDnoan 	
	0x003C004A, // PSx 	
	0x003D18A4, // SPDSonox 	
	0x003E1B24, // SPDSnaox 	
	0x003F00EA, // PSan 	
	0x00400F0A, // PSDnaa 	
	0x00410249, // DPSxon 	
	0x00420D5D, // SDxPDxa 	
	0x00431CC4, // SPDSanaxn 	
	0x00440328, // SDna 	// 44 SRCERASE
	0x00450B29, // DPSnaon 	
	0x004606C6, // DSPDaox 	
	0x0047076A, // PSDPxaxn 	
	0x00480368, // SDPxa 	
	0x004916C5, // PDSPDaoxxn 	
	0x004A0789, // DPSDoax 	
	0x004B0605, // PDSnox 	
	0x004C0CC8, // SDPana 	
	0x004D1954, // SSPxDSxoxn 	
	0x004E0645, // PDSPxox 	
	0x004F0E25, // PDSnoan 	
	0x00500325, // PDna 	
	0x00510B26, // DSPnaon 	
	0x005206C9, // DPSDaox 	
	0x00530764, // SPDSxaxn 	
	0x005408A9, // DPSonon 	
	0x00550009, // Dn 	// 55 DSTINVERT
	0x005601A9, // DPSox 	
	0x00570389, // DPSoan 	
	0x00580785, // PDSPoax 	
	0x00590609, // DPSnox 	
	0x005A0049, // DPx 	// 5A PATINVERT
	0x005B18A9, // DPSDonox 	
	0x005C0649, // DPSDxox 	
	0x005D0E29, // DPSnoan 	
	0x005E1B29, // DPSDnaox 	
	0x005F00E9, // DPan 	
	0x00600365, // PDSxa 	
	0x006116C6, // DSPDSaoxxn 	
	0x00620786, // DSPDoax 	
	0x00630608, // SDPnox 	
	0x00640788, // SDPSoax 	
	0x00650606, // DSPnox 	
	0x00660046, // DSx 	// 66 SRCINVERT
	0x006718A8, // SDPSonox 	
	0x006858A6, // DSPDSonoxxn 	
	0x00690145, // PDSxxn 	
	0x006A01E9, // DPSax 	
	0x006B178A, // PSDPSoaxxn 	
	0x006C01E8, // SDPax 	
	0x006D1785, // PDSPDoaxxn 	
	0x006E1E28, // SDPSnoax 	
	0x006F0C65, // PDSxnan 	
	0x00700CC5, // PDSana 	
	0x00711D5C, // SSDxPDxaxn 	
	0x00720648, // SDPSxox 	
	0x00730E28, // SDPnoan 	
	0x00740646, // DSPDxox 	
	0x00750E26, // DSPnoan 	
	0x00761B28, // SDPSnaox 	
	0x007700E6, // DSan 	
	0x007801E5, // PDSax 	
	0x00791786, // DSPDSoaxxn 	
	0x007A1E29, // DPSDnoax 	
	0x007B0C68, // SDPxnan 	
	0x007C1E24, // SPDSnoax 	
	0x007D0C69, // DPSxnan 	
	0x007E0955, // SPxDSxo 	
	0x007F03C9, // DPSaan 	
	0x008003E9, // DPSaa 	
	0x00810975, // SPxDSxon 	
	0x00820C49, // DPSxna 	
	0x00831E04, // SPDSnoaxn 	
	0x00840C48, // SDPxna 	
	0x00851E05, // PDSPnoaxn 	
	0x008617A6, // DSPDSoaxx 	
	0x008701C5, // PDSaxn 	
	0x008800C6, // DSa 	// 88 SRCAND
	0x00891B08, // SDPSnaoxn 	
	0x008A0E06, // DSPnoa 	
	0x008B0666, // DSPDxoxn 	
	0x008C0E08, // SDPnoa 	
	0x008D0668, // SDPSxoxn 	
	0x008E1D7C, // SSDxPDxax 	
	0x008F0CE5, // PDSanan 	
	0x00900C45, // PDSxna 	
	0x00911E08, // SDPSnoaxn 	
	0x009217A9, // DPSDPoaxx 	
	0x009301C4, // SPDaxn 	
	0x009417AA, // PSDPSoaxx 	
	0x009501C9, // DPSaxn 	
	0x00960169, // DPSxx 	
	0x0097588A, // PSDPSonoxx 	
	0x00981888, // SDPSonoxn 	
	0x00990066, // DSxn 	
	0x009A0709, // DPSnax 	
	0x009B07A8, // SDPSoaxn 	
	0x009C0704, // SPDnax 	
	0x009D07A6, // DSPDoaxn 	
	0x009E16E6, // DSPDSaoxx 	
	0x009F0345, // PDSxan 	
	0x00A000C9, // DPa 	
	0x00A11B05, // PDSPnaoxn 	
	0x00A20E09, // DPSnoa 	
	0x00A30669, // DPSDxoxn 	
	0x00A41885, // PDSPonoxn 	
	0x00A50065, // PDxn 	
	0x00A60706, // DSPnax 	
	0x00A707A5, // PDSPoaxn 	
	0x00A803A9, // DPSoa 	
	0x00A90189, // DPSoxn 	
	0x00AA0029, // D 	// AA DSTCOPY
	0x00AB0889, // DPSono 	
	0x00AC0744, // SPDSxax 	
	0x00AD06E9, // DPSDaoxn 	
	0x00AE0B06, // DSPnao 	
	0x00AF0229, // DPno 	
	0x00B00E05, // PDSnoa 	
	0x00B10665, // PDSPxoxn 	
	0x00B21974, // SSPxDSxox 	
	0x00B30CE8, // SDPanan 	
	0x00B4070A, // PSDnax 	
	0x00B507A9, // DPSDoaxn 	
	0x00B616E9, // DPSDPaoxx 	
	0x00B70348, // SDPxan 	
	0x00B8074A, // PSDPxax 	
	0x00B906E6, // DSPDaoxn 	
	0x00BA0B09, // DPSnao 	
	0x00BB0226, // DSno 	// BB MERGEPAINT
	0x00BC1CE4, // SPDSanax 	
	0x00BD0D7D, // SDxPDxan 	
	0x00BE0269, // DPSxo 	
	0x00BF08C9, // DPSano 	
	0x00C000CA, // PSa 	// C0 MERGECOPY
	0x00C11B04, // SPDSnaoxn 	
	0x00C21884, // SPDSonoxn 	
	0x00C3006A, // PSxn 	
	0x00C40E04, // SPDnoa 	
	0x00C50664, // SPDSxoxn 	
	0x00C60708, // SDPnax 	
	0x00C707AA, // PSDPoaxn 	
	0x00C803A8, // SDPoa 	
	0x00C90184, // SPDoxn 	
	0x00CA0749, // DPSDxax 	
	0x00CB06E4, // SPDSaoxn 	
	0x00CC0020, // S 	// CC SRCCOPY
	0x00CD0888, // SDPono 	
	0x00CE0B08, // SDPnao 	
	0x00CF0224, // SPno 	
	0x00D00E0A, // PSDnoa 	
	0x00D1066A, // PSDPxoxn 	
	0x00D20705, // PDSnax 	
	0x00D307A4, // SPDSoaxn 	
	0x00D41D78, // SSPxPDxax 	
	0x00D50CE9, // DPSanan 	
	0x00D616EA, // PSDPSaoxx 	
	0x00D70349, // DPSxan 	
	0x00D80745, // PDSPxax 	
	0x00D906E8, // SDPSaoxn 	
	0x00DA1CE9, // DPSDanax 	
	0x00DB0D75, // SPxDSxan 	
	0x00DC0B04, // SPDnao 	
	0x00DD0228, // SDno 	
	0x00DE0268, // SDPxo 	
	0x00DF08C8, // SDPano 	
	0x00E003A5, // PDSoa 	
	0x00E10185, // PDSoxn 	
	0x00E20746, // DSPDxax 	
	0x00E306EA, // PSDPaoxn 	
	0x00E40748, // SDPSxax 	
	0x00E506E5, // PDSPaoxn 	
	0x00E61CE8, // SDPSanax 	
	0x00E70D79, // SPxPDxan 	
	0x00E81D74, // SSPxDSxax 	
	0x00E95CE6, // DSPDSanaxxn 	
	0x00EA02E9, // DPSao 	
	0x00EB0849, // DPSxno 	
	0x00EC02E8, // SDPao 	
	0x00ED0848, // SDPxno 	
	0x00EE0086, // DSo 	// EE SRCPAINT
	0x00EF0A08, // SDPnoo 	
	0x00F00021, // P 	// F0 PATCOPY
	0x00F10885, // PDSono 	
	0x00F20B05, // PDSnao 	
	0x00F3022A, // PSno 	
	0x00F40B0A, // PSDnao 	
	0x00F50225, // PDno 	
	0x00F60265, // PDSxo 	
	0x00F708C5, // PDSano 	
	0x00F802E5, // PDSao 	
	0x00F90845, // PDSxno 	
	0x00FA0089, // DPo 	
	0x00FB0A09, // DPSnoo 	// FB PATPAINT
	0x00FC008A, // PSo 	
	0x00FD0A0A, // PSDnoo 	
	0x00FE02A9, // DPSoo 	
	0x00FF0062  // _WHITE // FF WHITENESS
};
#endif

	// PATPAINT 
	// MERGECOPY
	// SRCINVERT
	// SRCCOPY
	// 0xAA00EC
	// 0x00EC02E8

#if DEBUG_FONT_ROP
	static iRop4 = 0;
#endif

//===========================================================================

HDC GetDebuggerMemDC(void)
{
	if (!g_hDebuggerMemDC)
	{
		Win32Frame& win32Frame = Win32Frame::GetWin32Frame();

		HDC hFrameDC = win32Frame.FrameGetDC();
		g_hDebuggerMemDC = CreateCompatibleDC(hFrameDC);

		// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
		g_pDebuggerMemFramebufferinfo = (LPBITMAPINFO) new BYTE[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];

		memset(g_pDebuggerMemFramebufferinfo, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
		g_pDebuggerMemFramebufferinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		g_pDebuggerMemFramebufferinfo->bmiHeader.biWidth = 560;
		g_pDebuggerMemFramebufferinfo->bmiHeader.biHeight = 384;
		g_pDebuggerMemFramebufferinfo->bmiHeader.biPlanes = 1;
		g_pDebuggerMemFramebufferinfo->bmiHeader.biBitCount = 32;
		g_pDebuggerMemFramebufferinfo->bmiHeader.biCompression = BI_RGB;
		g_pDebuggerMemFramebufferinfo->bmiHeader.biClrUsed = 0;


		// CREATE THE FRAME BUFFER DIB SECTION
		g_hDebuggerMemBM = CreateDIBSection(
			hFrameDC,
			g_pDebuggerMemFramebufferinfo,
			DIB_RGB_COLORS,
			(LPVOID*)&g_pDebuggerMemFramebits, 0, 0
		);
		SelectObject(g_hDebuggerMemDC, g_hDebuggerMemBM);
	}

	_ASSERT(g_hDebuggerMemDC);	// TC: Could this be NULL?
	return g_hDebuggerMemDC;
}

void ReleaseDebuggerMemDC(void)
{
	if (g_hDebuggerMemDC)
	{
		DeleteObject(g_hDebuggerMemBM);
		g_hDebuggerMemBM = NULL;
		DeleteDC(g_hDebuggerMemDC);
		g_hDebuggerMemDC = NULL;

		Win32Frame& win32Frame = Win32Frame::GetWin32Frame();
		win32Frame.FrameReleaseDC();

		delete [] g_pDebuggerMemFramebufferinfo;
		g_pDebuggerMemFramebufferinfo = NULL;
		g_pDebuggerMemFramebits = NULL;
	}
}


HDC GetConsoleFontDC(void)
{
	if (!g_hConsoleFontDC)
	{
		Win32Frame& win32Frame = Win32Frame::GetWin32Frame();
		HDC hFrameDC = win32Frame.FrameGetDC();
		g_hConsoleFontDC = CreateCompatibleDC(hFrameDC);

		// CREATE A BITMAPINFO STRUCTURE FOR THE FRAME BUFFER
		g_hConsoleFontFramebufferinfo = (LPBITMAPINFO) new BYTE[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)];

		memset(g_hConsoleFontFramebufferinfo, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
		g_hConsoleFontFramebufferinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		g_hConsoleFontFramebufferinfo->bmiHeader.biWidth = CONSOLE_FONT_BITMAP_WIDTH;
		g_hConsoleFontFramebufferinfo->bmiHeader.biHeight = CONSOLE_FONT_BITMAP_HEIGHT;
		g_hConsoleFontFramebufferinfo->bmiHeader.biPlanes = 1;
		g_hConsoleFontFramebufferinfo->bmiHeader.biBitCount = 32;
		g_hConsoleFontFramebufferinfo->bmiHeader.biCompression = BI_RGB;
		g_hConsoleFontFramebufferinfo->bmiHeader.biClrUsed = 0;


		// CREATE THE FRAME BUFFER DIB SECTION
		g_hConsoleFontBitmap = CreateDIBSection(
			hFrameDC,
			g_hConsoleFontFramebufferinfo,
			DIB_RGB_COLORS,
			(LPVOID*)&g_hConsoleFontFramebits, 0, 0
		);
		SelectObject(g_hConsoleFontDC, g_hConsoleFontBitmap);

		// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
		HDC tmpDC = CreateCompatibleDC(hFrameDC);
		// Pre-scaled bitmap
		HBITMAP tmpFont = LoadBitmap(win32Frame.g_hInstance, TEXT("IDB_DEBUG_FONT_7x8"));  // Bitmap must be 112x128 as defined above
		SelectObject(tmpDC, tmpFont);
		BitBlt(g_hConsoleFontDC, 0, 0, CONSOLE_FONT_BITMAP_WIDTH, CONSOLE_FONT_BITMAP_HEIGHT,
			tmpDC, 0, 0,
			SRCCOPY);
		DeleteDC(tmpDC);
		DeleteObject(tmpFont);

	}

	_ASSERT(g_hConsoleFontDC);
	return g_hConsoleFontDC;
}

void ReleaseConsoleFontDC(void)
{
	if (g_hConsoleFontDC)
	{
		DeleteDC( g_hConsoleFontDC );
		g_hConsoleFontDC = NULL;
		DeleteObject( g_hConsoleFontBitmap );
		g_hConsoleFontBitmap = NULL;

		delete [] g_hConsoleFontFramebufferinfo;
		g_hConsoleFontFramebufferinfo = NULL;
		g_hConsoleFontFramebits = NULL;
	}

	DeleteObject( g_hConsoleBrushFG );
	g_hConsoleBrushFG = NULL;
	DeleteObject( g_hConsoleBrushBG );
	g_hConsoleBrushBG = NULL;
}


void StretchBltMemToFrameDC(void)
{
	Win32Frame& win32Frame = Win32Frame::GetWin32Frame();

	int nViewportCX, nViewportCY;
	win32Frame.GetViewportCXCY(nViewportCX, nViewportCY);

	int xdest = win32Frame.IsFullScreen() ? win32Frame.GetFullScreenOffsetX() : GetVideo().GetFrameBufferCentringOffsetX() * win32Frame.GetViewportScale();
	int ydest = win32Frame.IsFullScreen() ? win32Frame.GetFullScreenOffsetY() : GetVideo().GetFrameBufferCentringOffsetY() * win32Frame.GetViewportScale();
	int wdest = nViewportCX;
	int hdest = nViewportCY;

	BOOL bRes = StretchBlt(
		win32Frame.FrameGetDC(),			                // HDC hdcDest,
		xdest, ydest,									    // int nXOriginDest, int nYOriginDest,
		wdest, hdest,										// int nWidthDest,   int nHeightDest,
		GetDebuggerMemDC(),									// HDC hdcSrc,
		0, 0,												// int nXOriginSrc,  int nYOriginSrc,
		GetVideo().GetFrameBufferBorderlessWidth(), GetVideo().GetFrameBufferBorderlessHeight(),	// int nWidthSrc,    int nHeightSrc,
		SRCCOPY                                             // uint32_t dwRop
	);
}

// Font: Apple Text
//===========================================================================
void DebuggerSetColorFG( COLORREF nRGB )
{
#if USE_APPLE_FONT
	if (g_hConsoleBrushFG)
	{
		SelectObject( GetDebuggerMemDC(), GetStockObject(NULL_BRUSH) );
		DeleteObject( g_hConsoleBrushFG );
		g_hConsoleBrushFG = NULL;
	}

	g_hConsoleBrushFG = CreateSolidBrush(nRGB);

	g_cConsoleBrushFG_r = nRGB & 0xFF;
	g_cConsoleBrushFG_g = (nRGB>>8) & 0xFF;
	g_cConsoleBrushFG_b = (nRGB>>16) & 0xFF;

#else
	SetTextColor( GetDebuggerMemDC(), nRGB );
#endif
}

//===================================================
void DebuggerSetColorBG( COLORREF nRGB, bool bTransparent )
{
#if USE_APPLE_FONT
	if (g_hConsoleBrushBG)
	{
		SelectObject( GetDebuggerMemDC(), GetStockObject(NULL_BRUSH) );
		DeleteObject( g_hConsoleBrushBG );
		g_hConsoleBrushBG = NULL;
	}

	if (! bTransparent)
	{
		g_hConsoleBrushBG = CreateSolidBrush( nRGB );
	}

	// Transparency seems to be never used...
	g_cConsoleBrushBG_r = nRGB & 0xFF;
	g_cConsoleBrushBG_g = (nRGB >> 8) & 0xFF;
	g_cConsoleBrushBG_b = (nRGB >> 16) & 0xFF;

#else
	SetBkColor( GetDebuggerMemDC(), nRGB );
#endif
}

// @param glyph Specifies a native glyph from the 16x16 chars Apple Font Texture.
//===========================================================================
static void PrintGlyph( const int xDst, const int yDst, const int glyph )
{	
	HDC hDstDC = GetDebuggerMemDC();

	int xSrc = (glyph % CONSOLE_FONT_NUM_CHARS_PER_ROW) * CONSOLE_FONT_GRID_X;
	int ySrc = (glyph / CONSOLE_FONT_NUM_CHARS_PER_ROW) * CONSOLE_FONT_GRID_Y;
	_ASSERT(ySrc < CONSOLE_FONT_BITMAP_HEIGHT);

	// BUG #239 - (Debugger) Save debugger "text screen" to clipboard / file
	//	if ( g_bDebuggerVirtualTextCapture )
	// 
	{
#if _DEBUG
		if ((xDst < 0) || (yDst < 0))
			GetFrame().FrameMessageBox("X or Y out of bounds!", "PrintGlyph()", MB_OK );
#endif
		int col = xDst / CONSOLE_FONT_WIDTH ;
		int row = yDst / CONSOLE_FONT_HEIGHT;
		
		// if ( !g_bDebuggerCopyInfoPane )
		//    if ( col < 50
		if (xDst > DISPLAY_DISASM_RIGHT) // INFO_COL_2 // DISPLAY_CPU_INFO_LEFT_COLUMN
			col++;

		if ((col < DEBUG_VIRTUAL_TEXT_WIDTH)
		&&  (row < DEBUG_VIRTUAL_TEXT_HEIGHT))
			g_aDebuggerVirtualTextScreen[ row ][ col ] = glyph;
	}

	// Manual print of character. A lot faster than BitBlt, which must be avoided.
	int index_src = (CONSOLE_FONT_BITMAP_HEIGHT - 1 - ySrc) * CONSOLE_FONT_NUM_CHARS_PER_ROW * CONSOLE_FONT_GRID_X + xSrc;   // font bitmap
	int index_dst = (DISPLAY_HEIGHT - 1 - yDst) * DEBUG_VIRTUAL_TEXT_WIDTH * CONSOLE_FONT_GRID_X + xDst;   // debugger bitmap

	for (int yy = 0; yy < CONSOLE_FONT_GRID_Y; yy++)
	{
		for (int xx = 0; xx < CONSOLE_FONT_GRID_X; xx++)
		{
			char fontpx = g_hConsoleFontFramebits[index_src + xx].g;   // Should be same for R/G/B anyway (greyscale)
			g_pDebuggerMemFramebits[index_dst + xx].r = (g_cConsoleBrushBG_r & ~fontpx) | (g_cConsoleBrushFG_r & fontpx);
			g_pDebuggerMemFramebits[index_dst + xx].g = (g_cConsoleBrushBG_g & ~fontpx) | (g_cConsoleBrushFG_g & fontpx);
			g_pDebuggerMemFramebits[index_dst + xx].b = (g_cConsoleBrushBG_b & ~fontpx) | (g_cConsoleBrushFG_b & fontpx);
		}
		index_src -= CONSOLE_FONT_NUM_CHARS_PER_ROW * CONSOLE_FONT_GRID_X;
		index_dst -= DEBUG_VIRTUAL_TEXT_WIDTH * CONSOLE_FONT_GRID_X;
	}
}


//===========================================================================
void DebuggerPrint ( int x, int y, const char *pText )
{
	const int nLeft = x;

	char c;
	const char *p = pText;

	while ((c = *p))
	{
		if (c == '\n')
		{
			x = nLeft;
			y += CONSOLE_FONT_HEIGHT;
			p++;
			continue;
		}
		c &= 0x7F;
		PrintGlyph( x, y, c );
		x += CONSOLE_FONT_WIDTH;
		p++;
	}
}

//===========================================================================
void DebuggerPrintColor( int x, int y, const conchar_t * pText )
{
	int nLeft = x;

	conchar_t g;
	const conchar_t *pSrc = pText;

	if ( !pText)
		return;

	while ((g = (*pSrc)))
	{
		if (g == '\n')
		{
			x = nLeft;
			y += CONSOLE_FONT_HEIGHT;
			pSrc++;
			continue;
		}

		if (ConsoleColor_IsColorOrMouse( g ))
		{
			if (ConsoleColor_IsColor( g ))
			{
				DebuggerSetColorFG( ConsoleColor_GetColor( g ) );
			}

			g = ConsoleChar_GetChar( g );
		}

		PrintGlyph( x, y, (char) (g & _CONSOLE_COLOR_MASK) );
		x += CONSOLE_FONT_WIDTH;
		pSrc++;
	}
}


// Utility ________________________________________________________________________________________


//===========================================================================
bool CanDrawDebugger()
{
	if (DebugGetVideoMode(NULL))
		return false;

	if ((g_nAppMode == MODE_DEBUG) || (g_nAppMode == MODE_STEPPING))
		return true;

	return false;
}


//===========================================================================
int PrintText ( const char * pText, RECT & rRect )
{
#if _DEBUG
	if (! pText)
		GetFrame().FrameMessageBox("pText = NULL!", "DrawText()", MB_OK );
#endif

	int nLen = strlen( pText );

#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillBackground(rRect.left, rRect.top, rRect.right, rRect.bottom);
#endif

	DebuggerPrint( rRect.left, rRect.top, pText );
	return nLen;
}

//===========================================================================
void PrintTextColor ( const conchar_t *pText, RECT & rRect )
{
#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillBackground(rRect.left, rRect.top, rRect.right, rRect.bottom);
#endif

	DebuggerPrintColor( rRect.left, rRect.top, pText );
}

//===========================================================================
void FillBackground(long left, long top, long right, long bottom)
{
	long index_dst = (384-bottom) * 80 * CONSOLE_FONT_GRID_X;

	for (long x = left; x < right; x++)
	{
		g_pDebuggerMemFramebits[index_dst + x].r = g_cConsoleBrushBG_r;
		g_pDebuggerMemFramebits[index_dst + x].g = g_cConsoleBrushBG_g;
		g_pDebuggerMemFramebits[index_dst + x].b = g_cConsoleBrushBG_b;
	}

	if (top != bottom)
	{
		bgra_t* src = g_pDebuggerMemFramebits + (index_dst + left);
		bgra_t* dst = src + (80 * CONSOLE_FONT_GRID_X);
		size_t size = (right - left) * sizeof(bgra_t);
		for (int i = 0; i < bottom - top - 1; i++)
		{
			memcpy((void*)dst, (void*)src, size);
			dst += 80 * CONSOLE_FONT_GRID_X ;
		}
	}
}

// Updates the horizontal cursor
//===========================================================================
int PrintTextCursorX ( const char * pText, RECT & rRect )
{
	int nChars = 0;
	if (pText)
	{
		nChars = PrintText( pText, rRect );
		int nFontWidth = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
		rRect.left += (nFontWidth * nChars);
	}
	return nChars;
}

//===========================================================================
int PrintTextCursorY ( const char * pText, RECT & rRect )
{
	int nChars = PrintText( pText, rRect );
	rRect.top    += g_nFontHeight;
	rRect.bottom += g_nFontHeight;
	return nChars;
}





//===========================================================================
void SetupColorsHiLoBits ( bool bHighBit, bool bCtrlBit,
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

	DebuggerSetColorBG( DebuggerGetColor( iBackground ));
	DebuggerSetColorFG( DebuggerGetColor( iForeground ));

	if (bHighBit)
	{
		DebuggerSetColorBG( DebuggerGetColor( iColorHiBG ));
		DebuggerSetColorFG( DebuggerGetColor( iColorHiFG )); // was iForeground
	}

	if (bCtrlBit)
	{
		DebuggerSetColorBG( DebuggerGetColor( iColorLoBG ));
		DebuggerSetColorFG( DebuggerGetColor( iColorLoFG ));
	}
}


// To flush out color bugs... swap: iAsciBackground & iHighBackground
//===========================================================================
static std::string ColorizeSpecialChar( BYTE nData, const MemoryView_e iView,
	const int iTextBackground = BG_INFO     , const int iTextForeground = FG_DISASM_CHAR,
	const int iHighBackground = BG_INFO_CHAR, const int iHighForeground = FG_INFO_CHAR_HI,
	const int iCtrlBackground = BG_INFO_CHAR, const int iCtrlForeground = FG_INFO_CHAR_LO )
{
	bool bHighBit = false;
	bool bCtrlBit = false;

	int iTextBG = iTextBackground;
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

//	if (hDC)
	{
		SetupColorsHiLoBits( bHighBit, bCtrlBit
			, iTextBG, iTextFG // FG_DISASM_CHAR   
			, iHighBG, iHighFG // BG_INFO_CHAR     
			, iCtrlBG, iCtrlFG // FG_DISASM_OPCODE 
		);
	}

#if OLD_CONSOLE_COLOR
	if (ConsoleColorIsEscapeMeta( nChar ))
		return std::string( 2, nChar );
#endif

	return std::string( 1, nChar );
}

void ColorizeFlags( bool bSet, int bg_default = BG_INFO, int fg_default = FG_INFO_TITLE )
{
	if (bSet)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_INFO_INVERSE ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_INVERSE ));
	}
	else
	{
		DebuggerSetColorBG( DebuggerGetColor( bg_default ));
		DebuggerSetColorFG( DebuggerGetColor( fg_default ));
	}
}


// Main Windows ___________________________________________________________________________________


//===========================================================================
void DrawBreakpoints ( int line )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_BP_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

#if DISPLAY_BREAKPOINT_TITLE
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
	PrintText("Breakpoints", rect );
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
#endif

	int nBreakpointsDisplayed = 0;

	int iBreakpoint;
	for (iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++ )
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
		UINT nLength = pBP->nLength;

#if DEBUG_FORCE_DISPLAY
		nLength = 2;
#endif
		if (nLength)
		{
			bool bSet      = pBP->bSet;
			bool bEnabled  = pBP->bEnabled;
			WORD nAddress1 = pBP->nAddress;
			WORD nAddress2 = nAddress1 + nLength - 1;

#if DEBUG_FORCE_DISPLAY
//			if (iBreakpoint < MAX_DISPLAY_BREAKPOINTS_LINES)
				bSet = true;
#endif
			if (! bSet)
				continue;

			nBreakpointsDisplayed++;

//			if (nBreakpointsDisplayed > MAX_DISPLAY_BREAKPOINTS_LINES)
//				break;
			
			RECT rect2;
			rect2 = rect;
			
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "B", rect2 );

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ) );
			PrintTextCursorX( StrFormat("%X ", iBreakpoint).c_str(), rect2);

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ) );
//			PrintTextCursorX( ".", rect2 );

#if DEBUG_FORCE_DISPLAY
	pBP->eSource = (BreakpointSource_t) iBreakpoint;
#endif
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			int nRegLen = PrintTextCursorX( g_aBreakpointSource[ pBP->eSource ], rect2 );

			// Pad to 2 chars
			if (nRegLen < 2)
				rect2.left += g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ) );
#if DEBUG_FORCE_DISPLAY
	if (iBreakpoint < 3)
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint * 2);
//	else
//		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint-3 + BP_OP_READ);
#endif
			PrintTextCursorX( g_aBreakpointSymbols [ pBP->eOperator ], rect2 );

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

			DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );

#if DEBUG_FORCE_DISPLAY
	

	int iColor = R8 + iBreakpoint;
	COLORREF nColor = g_aColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BP_S_C ) );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_C );
	}
	DebuggerSetColorFG( nColor );
#endif

			PrintTextCursorX( WordToHexStr( nAddress1 ).c_str(), rect2);

			if (nLength == 1)
			{
				if (pBP->eSource == BP_SRC_MEM_READ_ONLY)
					PrintTextCursorX("R", rect2);
				else if (pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
					PrintTextCursorX("W", rect2);
			}

			if (nLength > 1)
			{
				DebuggerSetColorBG( DebuggerGetColor( BG_INFO ) );
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ) );

//				if (g_bConfigDisasmOpcodeSpaces)
//				{
//					PrintTextCursorX( " ", rect2 );
//					rect2.left += g_nFontWidthAvg;
//				}

				PrintTextCursorX( ":", rect2 );
//				rect2.left += g_nFontWidthAvg;
//				if (g_bConfigDisasmOpcodeSpaces) // TODO: Might have to remove spaces, for BPIO... addr-addr xx
//				{
//					rect2.left += g_nFontWidthAvg;
//				}

				DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
				DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
#if DEBUG_FORCE_DISPLAY
	COLORREF nColor = g_aColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		nColor = DebuggerGetColor( BG_INFO );
		DebuggerSetColorBG( nColor );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_X );
	}
	DebuggerSetColorFG( nColor );
#endif
				PrintTextCursorX( WordToHexStr( nAddress2 ).c_str(), rect2);

				if (pBP->eSource == BP_SRC_MEM_READ_ONLY)
					PrintTextCursorX("R", rect2);
				else if (pBP->eSource == BP_SRC_MEM_WRITE_ONLY)
					PrintTextCursorX("W", rect2);
			}

#if !USE_APPLE_FONT
			// Windows HACK: Bugfix: Rest of line is still breakpoint background color
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
			PrintTextCursorX( " ", rect2 );
#endif
			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;
		}
	}
}


// Console ________________________________________________________________________________________


//===========================================================================
int GetConsoleLineHeightPixels()
{
	int nHeight = g_aFontConfig[ FONT_CONSOLE ]._nFontHeight; // _nLineHeight; // _nFontHeight;
/*
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
*/
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
void DrawConsoleCursor ()
{
	DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_INPUT ));
	DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_INPUT ));

	int nWidth = g_aFontConfig[ FONT_CONSOLE ]._nFontWidthAvg;

	int nLineHeight = GetConsoleLineHeightPixels();
	int y = 0;

	const int nInputWidth = min( g_nConsoleInputChars, g_nConsoleInputScrollWidth ); // NOTE: Keep in Sync! DrawConsoleInput() and DrawConsoleCursor()

	RECT rect;
	rect.left   = (nInputWidth + g_nConsolePromptLen) * nWidth;
	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;
	rect.right  = rect.left + nWidth;

	PrintText( g_sConsoleCursor, rect );
}

//===========================================================================
void GetConsoleRect ( const int y, RECT & rect )
{
	int nLineHeight = GetConsoleLineHeightPixels();

	rect.left   = 0;
	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;

//	int nHeight = WindowGetHeight( g_iWindowThis );

	int nFontWidth = g_aFontConfig[ FONT_CONSOLE ]._nFontWidthAvg;
	int nMiniConsoleRight = g_nConsoleDisplayWidth * nFontWidth;
	int nFullConsoleRight = DISPLAY_WIDTH;
	int nRight = g_bConsoleFullWidth ? nFullConsoleRight : nMiniConsoleRight;
	rect.right = nRight;
}

//===========================================================================
void DrawConsoleLine ( const conchar_t * pText, int y )
{
	if (y < 0)
		return;

	RECT rect;
	GetConsoleRect( y, rect );

	// Console background is drawn in DrawWindowBackground_Info
	PrintTextColor( pText, rect );
}

//===========================================================================
void DrawConsoleInput ()
{
	DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_INPUT ));
	DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_INPUT ));

	RECT rect;
	GetConsoleRect( 0, rect );

	// For long input only show last g_nConsoleInputScrollWidth characters
	if (g_nConsoleInputChars > g_nConsoleInputScrollWidth)
	{
		assert(g_nConsoleInputScrollWidth <= CONSOLE_WIDTH); // NOTE: To support a wider input line the size of g_aConsoleInput[] must be increased

		//	g_nConsoleInputMaxLen      = 16;
		//	g_nConsoleInputScrollWidth = 10;
		//
		//  123456789ABCDEF  g_aConsoleInput[]
		//                 ^ g_nConsoleInputChars       = 15
		//  [--------]       g_nConsoleInputScrollWidth = 10
		// >6789ABCDEF_      g_nConsoleInputMaxLen      = 16
		static char aScrollingInput[ CONSOLE_WIDTH+1 ];
		aScrollingInput[0] = g_aConsoleInput[0];                                           // 1. Start-of-Line

		const int nInputOffset =      g_nConsoleInputChars - g_nConsoleInputScrollWidth  ; // 2. Middle
		const int nInputWidth  = min( g_nConsoleInputChars,  g_nConsoleInputScrollWidth ); // NOTE: Keep in Sync! DrawConsoleInput() and DrawConsoleCursor()
		strncpy( aScrollingInput+1, g_aConsoleInput + 1 + nInputOffset, nInputWidth );     // +1 to skip prompt

		aScrollingInput[ g_nConsoleInputScrollWidth+1 ] = 0;                               // 3. End-of-Line leave room for cursor

		PrintText( aScrollingInput, rect );
	}
	else
		 PrintText( g_aConsoleInput, rect );
}


//===========================================================================
WORD DrawDisassemblyLine ( int iLine, const WORD nBaseAddress )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return 0;

//	int iOpcode;
	int iOpmode;
	int nOpbyte;
	DisasmLine_t line;

	int iTable = NUM_SYMBOL_TABLES;
	std::string const* pSymbol = FindSymbolFromAddress( nBaseAddress, &iTable );
	const char* pMnemonic = NULL;

	// Data Disassembler
	int bDisasmFormatFlags = GetDisassemblyLine( nBaseAddress, line );
	const DisasmData_t *pData = line.pDisasmData;

//	iOpcode = line.iOpcode;	
	iOpmode = line.iOpmode;
	nOpbyte = line.nOpbyte;

	// sAddress, sOpcodes, sTarget, sTargetOffset, nTargetOffset, sTargetPointer, sTargetValue, sImmediate, nImmediate, sBranch );
	//> Address Separator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	//> xxxx: xx xx xx   LABEL    MNEMONIC    'E' =
	//>       ^          ^        ^           ^   ^
	//>       6          17       27          41  46
	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg

	enum TabStop_e
	{
		  TS_OPCODE
		, TS_LABEL
		, TS_INSTRUCTION
		, TS_IMMEDIATE
		, TS_BRANCH
		, TS_CHAR
		, _NUM_TAB_STOPS
	};

//	int X_OPCODE      =  6 * nDefaultFontWidth;
//	int X_LABEL       = 17 * nDefaultFontWidth;
//	int X_INSTRUCTION = 26 * nDefaultFontWidth; // 27
//	int X_IMMEDIATE   = 40 * nDefaultFontWidth; // 41
//	int X_BRANCH      = 46 * nDefaultFontWidth;

	float aTabs[ _NUM_TAB_STOPS ] =
#if USE_APPLE_FONT
      // xxxx:xx xx xx LABELxxxxxx MNEMONIC    'E' =
      // 0   45        14          26
	{ 5, 14, 26, 41, 48, 49 };
#else
	{ 5.75, 15.5, 25, 40.5, 45.5, 48.5 };
#endif

#if !USE_APPLE_FONT
	if (! g_bConfigDisasmAddressColon)
	{
		aTabs[ TS_OPCODE ] -= 1;
	}

	if ((g_bConfigDisasmOpcodesView) && (! g_bConfigDisasmOpcodeSpaces))
	{
		aTabs[ TS_LABEL       ] -= 3;
		aTabs[ TS_INSTRUCTION ] -= 2;
		aTabs[ TS_IMMEDIATE   ] -= 1;
	}
#endif	

	int iTab = 0;
	int nSpacer = 11; // 9
	for (iTab = 0; iTab < _NUM_TAB_STOPS; iTab++ )
	{
		if (! g_bConfigDisasmAddressView )
		{
			if (iTab < TS_IMMEDIATE) // TS_BRANCH)
			{
				aTabs[ iTab ] -= 4;
			}
		}
		if (! g_bConfigDisasmOpcodesView)
		{
			if (iTab < TS_IMMEDIATE) // TS_BRANCH)
			{
				aTabs[ iTab ] -= nSpacer;
				if (nSpacer > 0)
					nSpacer -= 2;
			}
		}

		aTabs[ iTab ] *= nDefaultFontWidth;
	}	

	int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight; // _nFontHeight; // g_nFontHeight

	RECT linerect;
	linerect.left   = 0;
	linerect.top    = iLine * nFontHeight;
	linerect.right  = DISPLAY_DISASM_RIGHT;
	linerect.bottom = linerect.top + nFontHeight;

	bool bBreakpointActive;
	bool bBreakpointEnable;
	GetBreakpointInfo( nBaseAddress, bBreakpointActive, bBreakpointEnable );
	bool bAddressAtPC = (nBaseAddress == regs.pc);
	int  bAddressIsBookmark = Bookmark_Find( nBaseAddress );

	DebugColors_e iBackground = BG_DISASM_1;
	DebugColors_e iForeground = FG_DISASM_ADDRESS;
	bool bCursorLine = false;

	if (((! g_bDisasmCurBad) && (iLine == g_nDisasmCurLine))
		|| (g_bDisasmCurBad && (iLine == 0)))
	{
		bCursorLine = true;

		// Breakpoint
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
			iForeground = FG_DISASM_ADDRESS;
		}
	}

	DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
	DebuggerSetColorFG( DebuggerGetColor( iForeground ) );

	if ( g_bConfigDisasmAddressView )
	{
		PrintTextCursorX( (LPCTSTR) line.sAddress, linerect );
	}


	// Address Separator
	if (! bCursorLine)
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );

	if (g_bConfigDisasmAddressColon)
	{
		if (bAddressIsBookmark)
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BOOKMARK ) );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BOOKMARK ) );

			// Can't use PrintTextCursorX() as that clamps chars > 0x7F to Mouse Text
			//    char bookmark_text[2] = { 0x7F + bAddressIsBookmark, 0 };
			//    PrintTextCursorX( bookmark_text, linerect );
			FillRect( GetDebuggerMemDC(), &linerect, g_hConsoleBrushBG );
			PrintGlyph( linerect.left, linerect.top, 0x7F + bAddressIsBookmark ); // Glyphs 0x80 .. 0x89 = Unicode U+24EA, U+2460 .. U+2468
			linerect.left += g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;

			DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
		}
		else
			PrintTextCursorX( ":", linerect );
	}
	else
		PrintTextCursorX( " ", linerect ); // bugfix, not showing "addr:" doesn't alternate color lines

	// Opcodes
	linerect.left = (int) aTabs[ TS_OPCODE ];

	if (! bCursorLine)
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );

	if (g_bConfigDisasmOpcodesView)
		PrintTextCursorX( (LPCTSTR) line.sOpCodes, linerect );

	// Label
	linerect.left = (int) aTabs[ TS_LABEL ];

	if (pSymbol)
	{
		if (! bCursorLine)
		{
			if (iTable == SYMBOLS_USER_2)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ) ); // Show user symbols 2 in different color for organization when reverse engineering.  Table 1 = known, Table 2 = unknown.
			else
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
		}
		PrintTextCursorX( pSymbol->c_str(), linerect );
	}

	// Instruction / Mnemonic
	linerect.left = (int) aTabs[ TS_INSTRUCTION ];

	if (! bCursorLine)
	{
		if ( pData ) // Assembler Data Directive / Data Disassembler
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_DIRECTIVE ) ); // TODO: FIXME: HACK? Is the color fine?
		else
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
	}

	pMnemonic = line.sMnemonic;
	PrintTextCursorX( pMnemonic, linerect );
	PrintTextCursorX( " ", linerect );

	// Target
	if (line.bTargetImmediate)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "#$", linerect );
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "(", linerect );
	}

	char *pTarget = line.sTarget;
	int nLen = strlen( pTarget );

	if (*pTarget == '$') // BUG? if ASC #:# starts with '$' ? // && (iOpcode != OPCODE_NOP)
	{
		pTarget++;
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "$", linerect );
	}

	if (! bCursorLine)
	{
		if (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL)
		{
			if (line.iTargetTable == SYMBOLS_USER_2)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ) ); // Show user symbols 2 in different color for organization when reverse engineering.  Table 1 = known, Table 2 = unknown.
			else
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
		}
		else
		{
			if (iOpmode == AM_M)
//			if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );
			}
			else
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_TARGET ) );
			}
		}
	}

	// https://github.com/AppleWin/AppleWin/issues/227
	// (Debugger)[1.25] AppleSoft symbol: COPY.FAC.TO.ARG.ROUNDED overflows into registers
	// Repro:
	//    UEA39
	// 2.8.0.1 Clamp excessive symbol target to not overflow
	//    SYM COPY.FAC.TO.ARG.ROUNDED = EB63
	// If opcodes aren't showing then length can be longer!
	// FormatOpcodeBytes() uses 3 chars/MAX_OPCODES. i.e. "## "
	int nMaxLen = DISASM_DISPLAY_MAX_TARGET_LEN;

	// 2.8.0.8: Bug #227: AppleSoft symbol: COPY.FAC.TO.ARG.ROUNDED overflows into registers
	if ( !g_bConfigDisasmAddressView )
	    nMaxLen += 4;
	if ( !g_bConfigDisasmOpcodesView )
	    nMaxLen += (DISASM_DISPLAY_MAX_OPCODES*3);

	// 2.9.0.9 Continuation of 2.8.0.8: Fix overflowing disassembly pane for long symbols
	int nOverflow = 0;
	if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
	{
		if (line.nTargetOffset != 0)
			nOverflow++;

		nOverflow += strlen( line.sTargetOffset );
	}

	if (line.bTargetIndirect || line.bTargetX || line.bTargetY)
	{
		if (line.bTargetX)
				nOverflow += 2;
		else
		if ((line.bTargetY) && (! line.bTargetIndirect))
				nOverflow += 2;
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
		nOverflow++;

	if (line.bTargetIndexed)
	{
		if (line.bTargetY)
			nOverflow += 2;
	}

	if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER)
	{
		nOverflow += strlen( line.sTargetPointer ); // '####'
		nOverflow ++  ;                             //     ':'
		nOverflow += 2;                             //      '##'
		nOverflow ++  ;                             //         ' '
	}

	if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
	{
		nOverflow += strlen( line.sImmediate );
	}

	if (nLen >=  (nMaxLen - nOverflow))
	{
#if _DEBUG
		// TODO: Warn on import about long symbol/target names
#endif
		pTarget[ nMaxLen - nOverflow ] = 0;
	}

	// TODO: FIXME: 2.8.0.7: Allow ctrl characters to show as inverse; i.e. ASC 400:40F
	PrintTextCursorX( pTarget, linerect );

	// Target Offset +/-
	if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		if (line.nTargetOffset > 0)
			PrintTextCursorX( "+", linerect );
		else
		if (line.nTargetOffset < 0)
			PrintTextCursorX( "-", linerect );

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE )); // Technically, not a hex number, but decimal
		}
		PrintTextCursorX( line.sTargetOffset, linerect );
	}
	// Inside parenthesis = Indirect Target Regs
	if (line.bTargetIndirect || line.bTargetX || line.bTargetY)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		if (line.bTargetX)
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "X", linerect );
		}
		else
		if ((line.bTargetY) && (! line.bTargetIndirect))
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "Y", linerect );
		}
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

		PrintTextCursorX( ")", linerect );
	}

	if (line.bTargetIndexed)
	{
		if (line.bTargetY)
		{
			PrintTextCursorX( ",", linerect );
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ) );
			PrintTextCursorX( "Y", linerect );
		}
	}

	// BUGFIX: 2.6.2.30:  DA $target --> show right paren
	if ( pData )
	{
		return nOpbyte;
	}

	// Memory Pointer and Value
	if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER) // (bTargetValue)
	{
		linerect.left = (int) aTabs[ TS_IMMEDIATE ]; // TS_IMMEDIATE ];

		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));

		PrintTextCursorX( line.sTargetPointer, linerect );

		if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_VALUE)
		{
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));

			if (g_iConfigDisasmTargets & DISASM_TARGET_BOTH)
				PrintTextCursorX( ":", linerect );

			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ));

			PrintTextCursorX( line.sTargetValue, linerect );
			PrintTextCursorX( " ", linerect );
		}
	}

	// 2.9.1.4: Print decimal for immediate values
	if (line.bTargetImmediate)
	{
		linerect.left = (int) aTabs[ TS_IMMEDIATE ];

		if ( line.nImmediate )
		{
			/*
                300:A9 80 A9 81 A9 FF A9 00 A9 01 A9 7E A9 7F
			*/

			// Right justify to target ADDR:##
			size_t len = strlen( line.sImmediateSignedDec );
			linerect.left += (2 + (4 - len)) * nDefaultFontWidth;

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( "#", linerect );

			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SINT8 ));
			PrintTextCursorX( line.sImmediateSignedDec, linerect);
		}
	}

	// Immediate Char
	if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
	{
		linerect.left = (int) aTabs[ TS_CHAR ]; // TS_IMMEDIATE ];

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		}

		if (! bCursorLine)
		{
			ColorizeSpecialChar( line.nImmediate, MEM_VIEW_ASCII, iBackground );
		}
		PrintTextCursorX( line.sImmediate, linerect );

		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // Hack: Colorize can "color bleed to EOL"
		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		}
	}

	// Branch Indicator		
	if (bDisasmFormatFlags & DISASM_FORMAT_BRANCH)
	{
		linerect.left = (int) aTabs[ TS_BRANCH ];

		if (! bCursorLine)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BRANCH ) );
		}

#if !USE_APPLE_FONT
		if (g_iConfigDisasmBranchType == DISASM_BRANCH_FANCY)
			SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont );  // g_hFontWebDings
#endif

		PrintText( line.sBranch, linerect );

#if !USE_APPLE_FONT
		if (g_iConfigDisasmBranchType)
			SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm
#endif
	}

	return nOpbyte;
}


//===========================================================================
static void DrawFlags ( int line, BYTE nRegFlags )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	// Regs are 10 chars across
	// Flags are 8 chars across -- scale "up"
	int nSpacerWidth = nFontWidth;

#if OLD_FLAGS_SPACING
	const int nScaledWidth = 10;
	if (nFontWidth)
		nSpacerWidth = (nScaledWidth * nFontWidth) / 8;
	nSpacerWidth++;
#endif

	//

	GetDebuggerMemDC();

	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_FLAG_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth);

	DebuggerSetColorBG( DebuggerGetColor( BG_DATA_1 )); // BG_INFO
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
	PrintText( "P ", rect );

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
	PrintText( ByteToHexStr( nRegFlags ).c_str(), rect);

	rect.top    -= g_nFontHeight;
	rect.bottom -= g_nFontHeight;

	rect.left += ((2 + _6502_NUM_FLAGS) * nSpacerWidth);
	rect.right = rect.left + nFontWidth;

	//

	int iFlag = 0;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		iFlag = (_6502_NUM_FLAGS - nFlag - 1);

		bool bSet = (nRegFlags & 1);


		if (bSet)
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO_INVERSE ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_INVERSE ));
		}
		else
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		}

		rect.left  -= nSpacerWidth;
		rect.right -= nSpacerWidth;
		const char szBpSrc[2] = { g_aBreakpointSource[ BP_SRC_FLAG_C + iFlag ][0], '\0' };
		PrintText( szBpSrc, rect );

		// Print Binary value
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

		const char szSet[2] = { '0' + static_cast<int>(bSet), '\0' };
		PrintText( szSet, rect );
		rect.top    -= g_nFontHeight;
		rect.bottom -= g_nFontHeight;

		nRegFlags >>= 1;
	}
}

//===========================================================================

void DrawByte_SY6522(std::string& sText, int iCol, WORD iAddress, BYTE data, bool timer1Active, bool timer2Active)
{
	sText = StrFormat("%02X", data);
	if (timer1Active && (iAddress == 4 || iAddress == 5))		// T1C
	{
		DebuggerSetColorFG(DebuggerGetColor(FG_INFO_TITLE));	// if timer1 active then draw in white
	}
	else if (timer2Active && (iAddress == 8 || iAddress == 9))	// T2C
	{
		DebuggerSetColorFG(DebuggerGetColor(FG_INFO_TITLE));	// if timer2 active then draw in white
	}
	else
	{
		if ((iCol & 1) == 0)
			DebuggerSetColorFG(DebuggerGetColor(FG_SY6522_EVEN));
		else
			DebuggerSetColorFG(DebuggerGetColor(FG_SY6522_ODD));
	}
}

void DrawByte_AY8913(std::string& sText, int iCol, WORD iAddress, BYTE data, BYTE nAYCurrentRegister)
{
	sText = StrFormat("%02X", data);
	if (nAYCurrentRegister == iAddress)
	{
		DebuggerSetColorFG(DebuggerGetColor(FG_INFO_TITLE));	// if latched address then draw in white
	}
	else
	{
		if ((iCol & 1) == 0)
			DebuggerSetColorFG(DebuggerGetColor(FG_AY8913_EVEN));
		else
			DebuggerSetColorFG(DebuggerGetColor(FG_AY8913_ODD));
	}
}

void DrawLine_MB_SUBUNIT(RECT& rect, WORD& iAddress, const int nCols, int iForeground, int iBackground, UINT subUnit, bool& is6522, MockingboardCard::DEBUGGER_MB_CARD& MB, bool isMockingboardInSlot)
{
	RECT rect2 = rect;

	for (int iCol = 0; iCol < nCols; iCol++)
	{
		DebuggerSetColorBG(DebuggerGetColor(iBackground));
		DebuggerSetColorFG(DebuggerGetColor(iForeground));

		std::string sText;

		if (isMockingboardInSlot && (is6522 || (!is6522 && iAddress <= 13)))
		{
			if (is6522)
			{
				BYTE data = MB.subUnit[subUnit].regsSY6522[iAddress];
				DrawByte_SY6522(sText, iCol, iAddress, data, MB.subUnit[subUnit].timer1Active, MB.subUnit[subUnit].timer2Active);
			}
			else
			{
				BYTE data = MB.subUnit[subUnit].regsAY8913[0][iAddress];
				DrawByte_AY8913(sText, iCol, iAddress, data, MB.subUnit[subUnit].nAYCurrentRegister[0]);
			}
		}
		else
		{
			sText = "--";	// No MB card in this slot; or AY regs 14 & 15 which aren't supported by AY-3-8913
			if (isMockingboardInSlot && !is6522 && iAddress == 15)	// for AY reg-15, output the AY's state
			{
				sText = (char*)&MB.subUnit[subUnit].szState[0];
				if (sText.compare("--") != 0)
					DebuggerSetColorFG(DebuggerGetColor(FG_AY8913_FUNCTION));	// Show any active function in red
			}
		}

		PrintTextCursorX(sText.c_str(), rect2);
		if (iCol == 3)
			PrintTextCursorX(":", rect2);

		iAddress++;
	}

	rect.top += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}

void DrawLine_AY8913_PAIR(RECT& rect, WORD& iAddress, const int nCols, int iForeground, int iBackground, UINT subUnit, UINT ay, MockingboardCard::DEBUGGER_MB_CARD& MB, bool isMockingboardInSlot)
{
	RECT rect2 = rect;

	for (int iCol = 0; iCol < nCols; iCol++)
	{
		DebuggerSetColorBG(DebuggerGetColor(iBackground));
		DebuggerSetColorFG(DebuggerGetColor(iForeground));

		std::string sText;

		if (isMockingboardInSlot && iAddress <= 13)
		{
			BYTE data = MB.subUnit[subUnit].regsAY8913[ay][iAddress];
			DrawByte_AY8913(sText, iCol, iAddress, data, MB.subUnit[subUnit].nAYCurrentRegister[ay]);
		}
		else
		{
			sText = "--";	// No MB card in this slot; or AY regs 14 & 15 which aren't supported by AY-3-8913
			if (isMockingboardInSlot && iAddress == 15)	// for AY reg-15, output the AY's state
			{
				sText = (char*)&MB.subUnit[subUnit].szState[ay];
				if (sText.compare("--") != 0)
					DebuggerSetColorFG(DebuggerGetColor(FG_AY8913_FUNCTION));	// Show any active function in red
			}
		}

		PrintTextCursorX(sText.c_str(), rect2);
		if (iCol == 3)
			PrintTextCursorX(":", rect2);

		iAddress++;
	}

	rect.top += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}

void DrawMemory ( int line, int iMemDump )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	bool bActive = pMD->bActive;
#if DEBUG_FORCE_DISPLAY
	bActive = true;
#endif
	if ( !bActive )
		return;

	USHORT       nAddr   = pMD->nAddress;
	DEVICE_e     eDevice = pMD->eDevice;
	MemoryView_e iView   = pMD->eView;

	MockingboardCard::DEBUGGER_MB_CARD MB;
	bool isMockingboardInSlot = false;
	UINT slot = nAddr >> 4, subUnit = nAddr & 1;

	if (eDevice == DEV_MB_SUBUNIT || eDevice == DEV_AY8913_PAIR)
	{
		if (GetCardMgr().GetMockingboardCardMgr().IsMockingboard(slot))
		{
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(slot)).GetSnapshotForDebugger(&MB);
			isMockingboardInSlot = true;
			for (int i = 0; i < NUM_SUBUNITS_PER_MB; i++)
			{
				for (int j = 0; j < NUM_AY8913_PER_SUBUNIT; j++)
				{
					if (!MB.subUnit[i].isAYLatchedAddressValid[j])
						MB.subUnit[i].nAYCurrentRegister[j] = 0xff;
				}
			}
		}
	}

	RECT rect = { 0 };
	rect.left   = DISPLAY_MINIMEM_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	RECT rect2 = rect;

	const int MAX_MEM_VIEW_TXT = 16;

	std::string sType = "Mem";
	if (eDevice == DEV_MB_SUBUNIT || eDevice == DEV_AY8913_PAIR)
		sType = StrFormat("Slot%d", slot);

	std::string sAddress;

	int iForeground = FG_INFO_OPCODE;
	int iBackground = BG_INFO;

#if DISPLAY_MEMORY_TITLE
	if (eDevice == DEV_MB_SUBUNIT)
	{
		sAddress = StrFormat("%c:%cSY & AY", 'A' + subUnit, !MB.subUnit[subUnit].is6522Bad ? ' ' : '!');
	}
	else if (eDevice == DEV_AY8913_PAIR)
	{
		sAddress = StrFormat("%c: AY1&AY2", 'A' + subUnit);
	}
	else
	{
		sAddress = WordToHexStr( nAddr );

		if (iView == MEM_VIEW_HEX)
			sType = "HEX";
		else if (iView == MEM_VIEW_ASCII)
			sType = "ASCII";
		else
			sType = "TEXT";
	}

	rect2 = rect;
	DebuggerSetColorFG(DebuggerGetColor(FG_INFO_TITLE));
	DebuggerSetColorBG(DebuggerGetColor(BG_INFO));
	PrintTextCursorX(sType.c_str(), rect2);

	DebuggerSetColorFG(DebuggerGetColor(FG_INFO_OPERATOR));
	if (eDevice == DEV_MB_SUBUNIT || eDevice == DEV_AY8913_PAIR)
		PrintTextCursorX(": ", rect2);
	else
		PrintTextCursorX(" at ", rect2);

	DebuggerSetColorFG(DebuggerGetColor(FG_INFO_ADDRESS));
	if (MB.subUnit[subUnit].is6522Bad && eDevice == DEV_MB_SUBUNIT)
		DebuggerSetColorFG(DebuggerGetColor(FG_INFO_ADDRESS_SY6522_AY8913_BAD));
	PrintTextCursorY(sAddress.c_str(), rect2);
#endif

	rect.top    = rect2.top;
	rect.bottom = rect2.bottom;

	WORD iAddress = nAddr;

	int nLines = g_nDisplayMemoryLines;
	int nCols = 4;

	if (iView != MEM_VIEW_HEX)
	{
		nCols = MAX_MEM_VIEW_TXT;
	}

	if (eDevice == DEV_MB_SUBUNIT || eDevice == DEV_AY8913_PAIR)
	{
		iAddress = 0;	// reg #0
		nCols = 8;
	}

	rect.right = DISPLAY_WIDTH - 1;

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));

	if (eDevice == DEV_MB_SUBUNIT)
	{
		// SlotX: A: SY & AY
		// 00010203:04050607	; SY
		// 08090A0B:0C0D0E0F
		// 00010203:04050607	; AY
		// 08090A0B:0C0D----

		bool is6522 = true;		// 1st is 6522

		for (int iLine = 0; iLine < nLines; iLine++)
		{
			DrawLine_MB_SUBUNIT(rect, iAddress, nCols, iForeground, iBackground, subUnit, is6522, MB, isMockingboardInSlot);

			if (iLine == 1)		// done lines 0 & 1, so advance to next subUnit
			{
				iBackground = BG_DATA_2;

				is6522 = false;	// 2nd is AY
				iAddress = 0;	// reg #0
			}
		}

		return;
	}

	if (eDevice == DEV_AY8913_PAIR)
	{
		// SlotX: A: AY1&AY2
		// 00010203:04050607	; AY1
		// 08090A0B:0C0D----
		// 00010203:04050607	; AY2
		// 08090A0B:0C0D----

		UINT ay = 0;	// 1st AY

		for (int iLine = 0; iLine < nLines; iLine++)
		{
			DrawLine_AY8913_PAIR(rect, iAddress, nCols, iForeground, iBackground, subUnit, ay, MB, isMockingboardInSlot);

			if (iLine == 1)		// done lines 0 & 1, so advance to next subUnit
			{
				iBackground = BG_DATA_2;

				ay = 1;			// 2nd AY
				iAddress = 0;	// reg #0
				if (MB.type != CT_Phasor)
					isMockingboardInSlot = false;
			}
		}

		return;
	}

	//

	for (int iLine = 0; iLine < nLines; iLine++)
	{
		rect2 = rect;

		if (iView == MEM_VIEW_HEX)
		{
			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_ADDRESS));
			PrintTextCursorX(WordToHexStr(iAddress).c_str(), rect2);

			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_OPERATOR));
			PrintTextCursorX(":", rect2);
		}

		for (int iCol = 0; iCol < nCols; iCol++)
		{
			DebuggerSetColorBG(DebuggerGetColor(iBackground));
			DebuggerSetColorFG(DebuggerGetColor(iForeground));

			std::string sText;

// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
//			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
//			{
//				sText = "IO ";
//			}
//			else
			{
				BYTE nData = (unsigned)*(LPBYTE)(mem + iAddress);

				if (iView == MEM_VIEW_HEX)
				{
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
					{
						DebuggerSetColorFG(DebuggerGetColor(FG_INFO_IO_BYTE));
					}

					sText = StrFormat("%02X ", nData);
				}
				else
				{
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
						iBackground = BG_INFO_IO_BYTE;

					sText = ColorizeSpecialChar(nData, iView, iBackground);
				}
			}
			PrintTextCursorX(sText.c_str(), rect2);
			iAddress++;
		}
		// Windows HACK: Bugfix: Rest of line is still background color
//		DebuggerSetColorBG(  hDC, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
//		DebuggerSetColorFG(hDC, DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
//		PrintTextCursorX( " ", rect2 );

		rect.top += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}

//===========================================================================
void DrawRegister ( int line, LPCTSTR name, const int nBytes, const WORD nValue, int iSource )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_REGS_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth); // + 1;

	if ((PARAM_REG_A  == iSource) ||
		(PARAM_REG_X  == iSource) ||
		(PARAM_REG_Y  == iSource) ||
		(PARAM_REG_PC == iSource) ||
		(PARAM_REG_SP == iSource))
	{		
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
	}
	else
	{
//		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		rect.left += nFontWidth;
	}

	// 2.6.0.0 Alpha - Regs not "boxed"
	int iBackground = BG_DATA_1;  // BG_INFO

	DebuggerSetColorBG( DebuggerGetColor( iBackground ));
	PrintText( name, rect );

	if (PARAM_REG_SP == iSource)
	{
		BYTE nStackDepth = _6502_STACK_END - (_6502_STACK_BEGIN + (nValue & 0xFF));
		int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
		rect.left += (2 * nFontWidth) + (nFontWidth >> 1); // 2.5 looks a tad nicer then 2

		// ## = Stack Depth (in bytes)
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR )); // FG_INFO_OPCODE, FG_INFO_TITLE
		PrintText( ByteToHexStr( nStackDepth ).c_str(), rect );
	}

	std::string sValue;

	if (nBytes == 2)
	{
		sValue = WordToHexStr( nValue );
	}
	else
	{
		assert(nBytes == 1 && nValue < 256); // Ensure the following downsizing is legal.
		const BYTE nData = BYTE(nValue);

		rect.left  = DISPLAY_REGS_COLUMN + (3 * nFontWidth);
//		rect.right = SCREENSPLIT2;

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX

		sValue = ColorizeSpecialChar( nData, MEM_VIEW_ASCII ); // MEM_VIEW_APPLE for inverse background little hard on the eyes

		DebuggerSetColorBG( DebuggerGetColor( iBackground ));
		PrintTextCursorX( sValue.c_str(), rect); // PrintTextCursorX()

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX()

		sValue = "  " + ByteToHexStr( nData );
	}

	// Needs to be far enough over, since 4 chars of ZeroPage symbol also calls us
	const int nOffset = 6;
	rect.left  = DISPLAY_REGS_COLUMN + (nOffset * nFontWidth);

	if ((PARAM_REG_PC == iSource) || (PARAM_REG_SP == iSource)) // Stack Pointer is target address, but doesn't look as good.
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
	}
	else
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // FG_DISASM_OPCODE
	}
	PrintText( sValue.c_str(), rect );
}

//===========================================================================
void DrawRegisters ( int line )
{
	const char **sReg = g_aBreakpointSource;

	DrawRegister( line++, sReg[ BP_SRC_REG_A ] , 1, regs.a , PARAM_REG_A  );
	DrawRegister( line++, sReg[ BP_SRC_REG_X ] , 1, regs.x , PARAM_REG_X  );
	DrawRegister( line++, sReg[ BP_SRC_REG_Y ] , 1, regs.y , PARAM_REG_Y  );
	DrawRegister( line++, sReg[ BP_SRC_REG_PC] , 2, regs.pc, PARAM_REG_PC );
	DrawFlags   ( line  , regs.ps);
	line += 2;
	DrawRegister( line++, sReg[ BP_SRC_REG_S ] , 2, regs.sp, PARAM_REG_SP );
}


// 2.9.0.3
//===========================================================================
void _DrawSoftSwitchHighlight( RECT & temp, bool bSet, const char *sOn, const char *sOff, int bg = BG_INFO )
{
//	DebuggerSetColorBG( DebuggerGetColor( bg                 ) ); // BG_INFO

	ColorizeFlags( bSet, bg );
	PrintTextCursorX( sOn, temp );

	DebuggerSetColorBG( DebuggerGetColor( bg                 ) ); // BG_INFO
	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
	PrintTextCursorX( "/", temp );

	ColorizeFlags( !bSet, bg );
	PrintTextCursorX( sOff, temp );
}


// 2.9.0.8
//===========================================================================
void _DrawSoftSwitchAddress( RECT & rect, int nAddress, int bg_default = BG_INFO )
{
	DebuggerSetColorBG( DebuggerGetColor( bg_default ));
	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_TARGET ));
	PrintTextCursorX( ByteToHexStr( nAddress & 0xFF ).c_str(), rect );

	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
	PrintTextCursorX( ":", rect );
}

// 2.7.0.7 Cleaned up display of soft-switches to show address.
//===========================================================================
void _DrawSoftSwitch( RECT & rect, int nAddress, bool bSet, const char *sPrefix, const char *sOn, const char *sOff, const char *sSuffix = NULL, int bg_default = BG_INFO )
{
	RECT temp = rect;

	_DrawSoftSwitchAddress( temp, nAddress, bg_default );

	if ( sPrefix )
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG )); // light blue
		PrintTextCursorX( sPrefix, temp );
	}

	// 2.9.0.3
	_DrawSoftSwitchHighlight( temp, bSet, sOn, sOff, bg_default );

	DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	if ( sSuffix )
		PrintTextCursorX( sSuffix, temp );

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}

// 2.9.0.8
//===========================================================================
void _DrawTriStateSoftSwitch( RECT & rect, int nAddress, const int iBankDisplay, int iActive, char *sPrefix, char *sOn, char *sOff, const char *sSuffix = NULL, int bg_default = BG_INFO )
{
//	if ((iActive == 0) || (iBankDisplay == iActive))
	bool bSet = (iBankDisplay == iActive);

	if ( bSet )
		_DrawSoftSwitch( rect, nAddress, bSet, NULL, sOn, sOff, " ", bg_default );
	else // Main Memory is active, or Bank # is not active
	{
		RECT temp = rect;
		int iBank = (GetMemMode() & MF_BANK2)
			? 2
			: 1
			;
		bool bDisabled = ((iActive == 0) && (iBank == iBankDisplay));


		_DrawSoftSwitchAddress( temp, nAddress, bg_default );

		// TODO: Q. Show which bank we are writing to in red?
		//       A. No, since we highlight bank 2 or 1, along with R/W
		DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
		if ( bDisabled )
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE  ) );
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );

		PrintTextCursorX( sOn , temp );
		PrintTextCursorX( "/" , temp );

		ColorizeFlags( bDisabled, bg_default, FG_DISASM_OPERATOR );
		PrintTextCursorX( sOff, temp );

		DebuggerSetColorBG( DebuggerGetColor( bg_default    ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
		PrintTextCursorX( " " , temp );

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}

/*
	Debugger: Support LC status and memory
	https://github.com/AppleWin/AppleWin/issues/406

	Bank2       Bank1       First Access, Second Access
	-----------------------------------------------
	C080        C088        Read RAM,     Write protect    MF_HIGHRAM   ~MF_WRITERAM
	C081        C089        Read ROM,     Write enable    ~MF_HIGHRAM    MF_WRITERAM
	C082        C08A        Read ROM,     Write protect   ~MF_HIGHRAM   ~MF_WRITERAM
	C083        C08B        Read RAM,     Write enable     MF_HIGHRAM    MF_WRITERAM
	c084        C08C        same as C080/C088
	c085        C08D        same as C081/C089
	c086        C08E        same as C082/C08A
	c087        C08F        same as C083/C08B
	MF_BANK2   ~MF_BANK2

	NOTE: Saturn 128K uses C084 .. C087 and C08C .. C08F to select Banks 0 .. 7 !
*/
// 2.9.0.4 Draw Language Card Bank Usage
// @param iBankDisplay Either 1 or 2
//===========================================================================
void _DrawSoftSwitchLanguageCardBank( RECT & rect, const int iBankDisplay, int bg_default = BG_INFO )
{
	const int w  = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
	const int dx80 = 7 * w; // "80:B2/MxR/W"
	//                          ^------^
	const int dx88 = 8 * w; // "88:B1/M    "
	//                          ^-------^

#ifdef _DEBUG
	const int finalRectRight = rect.left + 11 * w;
#endif

	rect.right = rect.left + dx80;

	// 0 = RAM
	// 1 = Bank 1
	// 2 = Bank 2
	bool bBankWritable = (GetMemMode() & MF_WRITERAM) ? 1 : 0;
	int iBankActive    = (GetMemMode() & MF_HIGHRAM)
		? (GetMemMode() & MF_BANK2)
			? 2
			: 1
		: 0
		;
	bool bBankOn = (iBankActive == iBankDisplay);

	// B#/[M]
	char sOn [ 4 ] = "B#"; // LC# but one char too wide :-/
	char sOff[ 4 ] = "M";
	// C080 LC2
	// C088 LC1
	int nAddress = 0xC080 + (8 * (2 - iBankDisplay));
	sOn[1] = '0' + iBankDisplay;

	// if off then ONLY highlight 'M' but only for the appropiate bank
	// if on  then do NOT highlight 'M'
	// if on  then also only highly the ACTIVE bank
	_DrawTriStateSoftSwitch( rect, nAddress, iBankDisplay, iBankActive, NULL, sOn, sOff, " ", bg_default );

	rect.top    -= g_nFontHeight;
	rect.bottom -= g_nFontHeight;

	if (iBankDisplay == 2)
	{
		rect.left   += dx80;
		rect.right  += 4*w;

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BP_S_X )); // Red
		DebuggerSetColorBG( DebuggerGetColor( bg_default       ));
		PrintTextCursorX( (GetMemMode() & MF_ALTZP) ? "x" : " ", rect );

		// [B2]/M R/[W]
		// [B2]/M [R]/W
		const char *pOn  = "R";
		const char *pOff = "W";

		_DrawSoftSwitchHighlight( rect, !bBankWritable, pOn, pOff, bg_default );
	}
	else
	{
		_ASSERT(iBankDisplay == 1);

		rect.left   += dx88;
		rect.right  += 4*w;

		int iActiveBank = -1;
		char cMemType = '?'; // Default to RAMWORKS
		if (GetCurrentExpansionMemType() == CT_RamWorksIII) { cMemType = 'r'; iActiveBank = GetRamWorksActiveBank(); }
		if (GetCurrentExpansionMemType() == CT_Saturn128K)  { cMemType = 's'; iActiveBank = GetCardMgr().GetLanguageCardMgr().GetLanguageCard()->GetActiveBank(); }

		if (iActiveBank >= 0)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG )); // light blue
			const char sMemType[2] = { cMemType, '\0' };
			PrintTextCursorX( sMemType, rect );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));	// orange
			PrintTextCursorX( ByteToHexStr( iActiveBank & 0x7F ).c_str(), rect );
		}
		else
		{
			PrintTextCursorX("   ", rect);
		}
	}

	_ASSERT(rect.right == finalRectRight);

	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
}


/*
	$C002 W RAMRDOFF  Read enable  main memory from $0200-$BFFF
	$C003 W RAMDRON   Read enable  aux  memory from $0200-$BFFF
	$C004 W RAMWRTOFF Write enable main memory from $0200-$BFFF
	$C005 W RAMWRTON  Write enable aux  memory from $0200-$BFFF
*/
// 2.9.0.6
// GH#406 https://github.com/AppleWin/AppleWin/issues/406
//===========================================================================
void _DrawSoftSwitchMainAuxBanks( RECT & rect, int bg_default = BG_INFO )
{
	RECT temp = rect;
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;


	/*
		Ideally, we'd show Read/Write for Main/Aux
			02:RM/X
			04:WM/X
		But we only have 1 line:
			02:Rm/x Wm/x
			00:ASC/MOUS
		Which is one character too much.
			02:Rm/x Wm/a
		But we abbreaviate it without the space and color code the Read and Write:
			02:Rm/xWm/x
	*/

	int w  = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
	int dx = 7 * w;

	int  nAddress  = 0xC002;
	bool bMainRead = (GetMemMode() & MF_AUXREAD)  ? true : false;
	bool bAuxWrite = (GetMemMode() & MF_AUXWRITE) ? true : false;

	temp.right = rect.left + dx;
	_DrawSoftSwitch( temp, nAddress, !bMainRead, "R", "m", "x", NULL, BG_DATA_2 );

	temp.top    -= g_nFontHeight;
	temp.bottom -= g_nFontHeight;
	temp.left   += dx;
	temp.right  += 3*w;

	DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BP_S_X )); // Red
	DebuggerSetColorBG( DebuggerGetColor( bg_default       ));
	PrintTextCursorX( "W", temp );
	_DrawSoftSwitchHighlight( temp, !bAuxWrite, "m", "x", BG_DATA_2 );
}


// 2.7.0.1 Display state of soft switches
//===========================================================================
void DrawSoftSwitches( int iSoftSwitch )
{
	RECT rect;
	RECT temp;
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	rect.left   = DISPLAY_SOFTSWITCH_COLUMN;
	rect.top    = iSoftSwitch * g_nFontHeight;
	rect.right = rect.left + (10 * nFontWidth) + 1;
	rect.bottom = rect.top + g_nFontHeight;
	temp = rect;

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	
#if SOFTSWITCH_OLD
	char sText[16] = "";
	// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
	// GR  / TEXT
	// GRAPH/TEXT
	// TEXT ON/OFF
	PrintTextCursorY( !VideoGetSWTEXT() ? "GR  / ----" : "--  / TEXT", rect );

	// $C052 / $C053 = MIXEDOFF/MIXEDON = SW.MIXCLR/SW.MIXSET
	// FULL/MIXED
	// MIX OFF/ON
	PrintTextCursorY( !VideoGetSWMIXED() ? "FULL/-----" : "----/MIXED", rect );

	// $C054 / $C055 = PAGE1/PAGE2 = PAGE2OFF/PAGE2ON = SW.LOWSCR/SW.HISCR
	// PAGE 1 / 2
	PrintTextCursorY( !VideoGetSWPAGE2() ? "PAGE 1 / -" : "PAGE - / 2", rect );
	
	// $C056 / $C057 LORES/HIRES = HIRESOFF/HIRESON = SW.LORES/SW.HIRES
	// LO / HIRES
	// LO / -----
	// -- / HIRES
	PrintTextCursorY( !VideoGetSWHIRES()     ? "LO /-- RES"  : "-- /HI RES" , rect ); PrintTextCursorY( "", rect );
	PrintTextCursorY( !VideoGetSW80COL()     ? "40 / -- COL" : "-- / 80 COL", rect ); // Extended soft switches
	PrintTextCursorY( VideoGetSWAltCharSet() ? "ASCII/-----" : "-----/MOUSE", rect );
	PrintTextCursorY( !VideoGetSWDHIRES()    ? "HGR / ----"  : "--- / DHGR" , rect ); // 280/560 HGR
#else //SOFTSWITCH_OLD
	// See: VideoSetMode()

	// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
	// GR  / TEXT
	// GRAPH/TEXT
	// TEXT ON/OFF
	bool bSet;

	// $C050 / $C051 = TEXTOFF/TEXTON = SW.TXTCLR/SW.TXTSET
	bSet = !GetVideo().VideoGetSWTEXT();
	_DrawSoftSwitch( rect, 0xC050, bSet, NULL, "GR.", "TEXT" );

	// $C052 / $C053 = MIXEDOFF/MIXEDON = SW.MIXCLR/SW.MIXSET
	// FULL/MIXED
	// MIX OFF/ON
	bSet = !GetVideo().VideoGetSWMIXED();
	_DrawSoftSwitch( rect, 0xC052, bSet, NULL, "FULL", "MIX" );

	// $C054 / $C055 = PAGE1/PAGE2 = PAGE2OFF/PAGE2ON = SW.LOWSCR/SW.HISCR
	// PAGE 1 / 2
	bSet = !GetVideo().VideoGetSWPAGE2();
	_DrawSoftSwitch( rect, 0xC054, bSet, "PAGE ", "1", "2" );
	
	// $C056 / $C057 LORES/HIRES = HIRESOFF/HIRESON = SW.LORES/SW.HIRES
	// LO / HIRES
	// LO / -----
	// -- / HIRES
	bSet = !GetVideo().VideoGetSWHIRES();
	_DrawSoftSwitch( rect, 0xC056, bSet, NULL, "LO", "HI", "RES" );

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

	// 280/560 HGR
	// C05E = ON, C05F = OFF
	bSet = GetVideo().VideoGetSWDHIRES();
	_DrawSoftSwitch( rect, 0xC05E, bSet, NULL, "DHGR", "HGR" );


	// Extended soft switches
	int bgMemory = BG_DATA_2;

	// C000 = 80STOREOFF, C001 = 80STOREON
	bSet = !GetVideo().VideoGetSW80STORE();
	_DrawSoftSwitch( rect, 0xC000, bSet, "80Sto", "0", "1", NULL, bgMemory );

	// C002 .. C005
	_DrawSoftSwitchMainAuxBanks( rect, bgMemory );

	// C00C = off, C00D = on
	bSet = !GetVideo().VideoGetSW80COL();
	_DrawSoftSwitch( rect, 0xC00C, bSet, "Col", "40", "80", NULL, bgMemory );

	// C00E = off, C00F = on
	bSet = !GetVideo().VideoGetSWAltCharSet();
	_DrawSoftSwitch( rect, 0xC00E, bSet, NULL, "ASC", "MOUS", NULL, bgMemory ); // ASCII/MouseText

#if SOFTSWITCH_LANGCARD
	// GH#406 https://github.com/AppleWin/AppleWin/issues/406
	// 2.9.0.4
	// Language Card Bank 1/2
	// See: MemSetPaging()

// LC2 & C008/C009 (ALTZP & ALT-LC)
	DebuggerSetColorBG( DebuggerGetColor( bgMemory )); // BG_INFO_2 -> BG_DATA_2
	_DrawSoftSwitchLanguageCardBank( rect, 2, bgMemory );

// LC1
	rect.left = DISPLAY_SOFTSWITCH_COLUMN; // INFO_COL_2;
	_DrawSoftSwitchLanguageCardBank( rect, 1, bgMemory );
#endif

#endif // SOFTSWITCH_OLD
}


//===========================================================================
void DrawSourceLine( int iSourceLine, RECT &rect )
{
	char sLine[ CONSOLE_WIDTH ];
	memset( sLine, 0, CONSOLE_WIDTH );

	if ((iSourceLine >=0) && (iSourceLine < g_AssemblerSourceBuffer.GetNumLines() ))
	{
		char * pSource = g_AssemblerSourceBuffer.GetLine( iSourceLine );

//		int nLenSrc = strlen( pSource );
//		if (nLenSrc >= CONSOLE_WIDTH)
//			bool bStop = true;

		TextConvertTabsToSpaces( sLine, pSource, CONSOLE_WIDTH-1 ); // bugfix 2,3,1,15: fence-post error, buffer over-run

//		int nLenTab = strlen( sLine );
	}
	else
	{
		strcpy( sLine, " " );
	}

	PrintText( sLine, rect );
	rect.top += g_nFontHeight;
}


//===========================================================================
void DrawStack ( int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	unsigned nAddress = regs.sp;
#if DEBUG_FORCE_DISPLAY // Stack
	nAddress = 0x100;
#endif

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	// 2.6.0.0 Alpha - Stack was dark cyan BG_DATA_1
	DebuggerSetColorBG( DebuggerGetColor( BG_DATA_1 )); // BG_INFO // recessed 3d look

	int    iStack = 0;
	while (iStack < MAX_DISPLAY_STACK_LINES)
	{
		nAddress++;

		RECT rect;
		rect.left   = DISPLAY_STACK_COLUMN;
		rect.top    = (iStack+line) * g_nFontHeight;
		rect.right = rect.left + (10 * nFontWidth) + 1;
		rect.bottom = rect.top + g_nFontHeight;

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); // [COLOR_STATIC

		if (nAddress <= _6502_STACK_END)
		{
			PrintTextCursorX( StrFormat( "%04X: ", nAddress ).c_str(), rect );
		}

		if (nAddress <= _6502_STACK_END)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // COLOR_FG_DATA_TEXT
			PrintTextCursorX( StrFormat( "  %02X", (unsigned)*(LPBYTE)(mem+nAddress) ).c_str(), rect );
		}
		iStack++;
	}
}


//===========================================================================
void DrawTargets ( int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int aTarget[3];
	_6502_GetTargets( regs.pc, &aTarget[0],&aTarget[1],&aTarget[2], NULL );
	GetTargets_IgnoreDirectJSRJMP(mem[regs.pc], aTarget[2]);

	aTarget[1] = aTarget[2];	// Move down as we only have 2 lines

	RECT rect;
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
	
	int iAddress = MAX_DISPLAY_TARGET_PTR_LINES;
	while (iAddress--)
	{
		// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
//		if ((aTarget[iAddress] >= _6502_IO_BEGIN) && (aTarget[iAddress] <= _6502_IO_END))
//			aTarget[iAddress] = NO_6502_TARGET;

		std::string sAddress = "-none-";
		std::string sData;

#if DEBUG_FORCE_DISPLAY // Targets
		if (aTarget[iAddress] == NO_6502_TARGET)
			aTarget[iAddress] = 0;
#endif
		if (aTarget[iAddress] != NO_6502_TARGET)
		{
			sAddress = WordToHexStr(aTarget[iAddress]);
			if (iAddress)
				sData = ByteToHexStr(*(LPBYTE)(mem+aTarget[iAddress]));
			else
				sData = WordToHexStr(*(LPWORD)(mem+aTarget[iAddress]));
		}

		rect.left   = DISPLAY_TARGETS_COLUMN;
		rect.top    = (line+iAddress) * g_nFontHeight;
		int nColumn = rect.left + (7 * nFontWidth);
		rect.right  = nColumn;
		rect.bottom = rect.top + g_nFontHeight;

		if (iAddress == 0)
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); // Temp Address
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS )); // Target Address

		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		PrintText( sAddress.c_str(), rect );

		rect.left  = nColumn;
		rect.right = rect.left + (10 * nFontWidth); // SCREENSPLIT2

		if (iAddress == 0)
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS )); // Temp Target
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // Target Bytes

		PrintText( sData.c_str(), rect );
	}
}

//===========================================================================
void DrawWatches (int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_WATCHES_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	DebuggerSetColorBG(DebuggerGetColor( BG_INFO_WATCH ));

#if DISPLAY_WATCH_TITLE
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	PrintTextCursorY("Watches", rect );
#endif

	int iWatch;
	for (iWatch = 0; iWatch < MAX_WATCHES; iWatch++ )
	{
#if DEBUG_FORCE_DISPLAY // Watch
		if (true)
#else
		if (g_aWatches[iWatch].bEnabled && g_aWatches[iWatch].eSource == BP_SRC_MEM_RW)
#endif
		{
			RECT rect2 = rect;

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "W", rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( StrFormat( "%X ", iWatch ).c_str(), rect2 );
			
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( WordToHexStr( g_aWatches[iWatch].nAddress ).c_str(), rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			//

			BYTE nTargetL = *(LPBYTE)(mem + g_aWatches[iWatch].nAddress);
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( ByteToHexStr( nTargetL ).c_str(), rect2 );

			BYTE nTargetH = *(LPBYTE)(mem + ((g_aWatches[iWatch].nAddress + 1) & 0xffff));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( ByteToHexStr( nTargetH ).c_str(), rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( "(", rect2 );

			WORD nTarget16 = (((WORD)nTargetH) << 8) | ((WORD)nTargetL);
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( WordToHexStr( nTarget16 ).c_str(), rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ")", rect2 );

			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;

// 1.19.4 Added: Watch show (dynamic) raw hex bytes
			rect2 = rect;

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			for ( int iByte = 0; iByte < 8; iByte++ )
			{
				if  ((iByte & 3) == 0) {
					DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
					PrintTextCursorX( " ", rect2 );
				}

				if ((iByte & 1) == 1)
					DebuggerSetColorBG( DebuggerGetColor( BG_INFO_WATCH ));
				else
					DebuggerSetColorBG( DebuggerGetColor( BG_DATA_2 ));

				BYTE nValue8 = mem[ (nTarget16 + iByte) & 0xffff ];
				PrintTextCursorX( ByteToHexStr( nValue8 ).c_str(), rect2 );
			}
		}
		else if (g_aWatches[iWatch].bEnabled && g_aWatches[iWatch].eSource == BP_SRC_VIDEO_SCANNER)
		{
			uint32_t data;
			int dataSize;
			g_aWatches[iWatch].nAddress = NTSC_GetScannerAddressAndData(data, dataSize);

			RECT rect2 = rect;

			DebuggerSetColorBG(DebuggerGetColor(BG_INFO_WATCH));
			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_TITLE));
			PrintTextCursorX("W", rect2);

			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_BULLET));
			PrintTextCursorX(StrFormat("%X ", iWatch).c_str(), rect2);

			DebuggerSetColorFG(DebuggerGetColor(FG_DISASM_ADDRESS));
			PrintTextCursorX(WordToHexStr(g_aWatches[iWatch].nAddress).c_str(), rect2);

			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_OPERATOR));
			PrintTextCursorX(":", rect2);

			DebuggerSetColorFG(DebuggerGetColor(FG_INFO_OPCODE));
			if (dataSize == 1)
			{
				PrintTextCursorX(ByteToHexStr(data).c_str(), rect2);
			}
			else if (dataSize == 2)
			{
				PrintTextCursorX("a:", rect2);
				PrintTextCursorX(ByteToHexStr(data>>8).c_str(), rect2);
				PrintTextCursorX(" m:", rect2);
				PrintTextCursorX(ByteToHexStr(data).c_str(), rect2);
			}
			else
			{
				_ASSERT(dataSize == 4);
				PrintTextCursorX(DWordToHexStr(data).c_str(), rect2);
			}

			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;
		}
		else
		{
			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;
		}

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}


//===========================================================================
void DrawZeroPagePointers ( int line )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.top    = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left   = DISPLAY_ZEROPAGE_COLUMN;
	rect.right  = rect.left + (10 * nFontWidth);

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO_ZEROPAGE ));

	const int nMaxSymbolLen = 7;

	for (int iZP = 0; iZP < MAX_ZEROPAGE_POINTERS; iZP++)
	{
		RECT rect2 = rect;

		Breakpoint_t *pZP = &g_aZeroPagePointers[iZP];
		bool bEnabled = pZP->bEnabled;

#if DEBUG_FORCE_DISPLAY // Zero-Page
		bEnabled = true;
#endif
		if (bEnabled)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "Z", rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( StrFormat( "%X ", iZP ).c_str(), rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( " ", rect2 );

			BYTE nZPAddr1 = (g_aZeroPagePointers[iZP].nAddress  ) & 0xFF; // +MJP missig: "& 0xFF", or "(BYTE) ..."
			BYTE nZPAddr2 = (g_aZeroPagePointers[iZP].nAddress+1) & 0xFF;

			std::string sAddressBuf1;
			std::string sAddressBuf2;
			std::string const& sSymbol1 = GetSymbol(nZPAddr1, 2, sAddressBuf1);	// 2:8-bit value (if symbol not found)
			std::string const& sSymbol2 = GetSymbol(nZPAddr2, 2, sAddressBuf2);	// 2:8-bit value (if symbol not found)

			std::string sText;

//			if ((sSymbol1.length() == 1) && (sSymbol2.length() == 1))
//				sText = sSymbol1 + sSymbol2;

			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));

			if (!sSymbol1.empty() && sSymbol1[0] == '$')
			{
//				sText = pSymbol1;
//				sZPAddr = WordToHexStr( nZPAddr1 );
			}
			else
			if (!sSymbol2.empty() && sSymbol2[0] == '$')
			{
//				sText = pSymbol2;
//				sZPAddr = WordToHexStr( nZPAddr2 );
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			}
			else
			{
				size_t nMin = MIN( sSymbol1.length(), size_t(nMaxSymbolLen) );
				sText.assign(sSymbol1.c_str(), nMin);
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
			}
//			DrawRegister( line+iZP, szZP, 2, nTarget16);
//			PrintText( szZP, rect2 );
			if (sText.length() < nMaxSymbolLen)
				sText.resize(nMaxSymbolLen, CHAR_SPACE);
			PrintText( sText.c_str(), rect2);

			rect2.left    = rect.left;
			rect2.top    += g_nFontHeight;
			rect2.bottom += g_nFontHeight;

			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( ByteToHexStr( nZPAddr1 ).c_str(), rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			WORD nTarget16 = (WORD)mem[ nZPAddr1 ] | ((WORD)mem[ nZPAddr2 ]<< 8);
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( WordToHexStr( nTarget16 ).c_str(), rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			BYTE nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16);
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( ByteToHexStr( nValue8 ).c_str(), rect2 );
		}
		rect.top    += (g_nFontHeight * 2);
		rect.bottom += (g_nFontHeight * 2);
	}
}


// Sub Windows ____________________________________________________________________________________

//===========================================================================
void DrawSubWindow_Console (Update_t bUpdate)
{
	if (! CanDrawDebugger())
		return;

#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_CONSOLE ]._hFont );
#endif

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY)
		|| (bUpdate & UPDATE_CONSOLE_INPUT))
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_CONSOLE_OUTPUT ));

//		int nLines = MIN(g_nConsoleDisplayTotal - g_iConsoleDisplayStart, g_nConsoleDisplayHeight);
		int iLine = g_iConsoleDisplayStart + CONSOLE_FIRST_LINE;
		for (int y = 1; y < g_nConsoleDisplayLines ; y++ )
		{
			if (iLine <= (g_nConsoleDisplayTotal + CONSOLE_FIRST_LINE))
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_CONSOLE_OUTPUT ));
				DrawConsoleLine( g_aConsoleDisplay[ iLine  ], y );
			}
			else
			{
				// bugfix: 2.6.1.34
				// scrolled past top of console... Draw blank line
				DrawConsoleLine( NULL, y );
			}
			iLine++;
		}

		DrawConsoleInput();
	}

//	if (bUpdate & UPDATE_CONSOLE_INPUT)
	{
//		DrawConsoleInput();
	}
}	

//===========================================================================
void DrawSubWindow_Data (Update_t bUpdate)
{
//	HDC hDC = g_hDC;
	int iBackground;

	const int nMaxOpcodes = WINDOW_DATA_BYTES_PER_LINE;

	_ASSERT( CONSOLE_WIDTH > (WINDOW_DATA_BYTES_PER_LINE * 3));

	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	const int X_OPCODE          =  6                    * nDefaultFontWidth;
	const int X_CHAR            = (6 + (nMaxOpcodes*3)) * nDefaultFontWidth;

	int iMemDump = 0;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	USHORT       nAddress = pMD->nAddress;

//	if (!pMD->bActive)
//		return;

//	int iWindows = g_iThisWindow;
//	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

	RECT rect = { 0 };
	rect.top = 0;

	WORD iAddress = nAddress;

	const int nLines = g_nDisasmWinHeight;

	for ( int iLine = 0; iLine < nLines; iLine++ )
	{
		iAddress = nAddress;

	// Format
		std::string sAddress = WordToHexStr( iAddress );

		std::string sOpcodes;
		const BYTE* mp = mem + iAddress;
		for ( int iByte = 0; iByte < nMaxOpcodes; ++iByte, ++mp )
		{
			StrAppendByteAsHex(sOpcodes, *mp);
			sOpcodes += ' ';
		}

		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight;

	// Draw
		rect.left   = 0;
		rect.right  = DISPLAY_DISASM_RIGHT;
		rect.bottom = rect.top + nFontHeight;

		iBackground = !!(iLine & 1) ? BG_DATA_1 : BG_DATA_2;

		DebuggerSetColorBG( DebuggerGetColor( iBackground ) );

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ) );
		PrintTextCursorX( sAddress.c_str(), rect);

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			PrintTextCursorX( ":", rect );

		rect.left = X_OPCODE;

		DebuggerSetColorFG( DebuggerGetColor( FG_DATA_BYTE ) );
		PrintTextCursorX( sOpcodes.c_str(), rect);

		rect.left = X_CHAR;

	// Separator
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );


	// Plain Text
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_CHAR ) );

		MemoryView_e eView = pMD->eView;
		if ((eView != MEM_VIEW_ASCII) && (eView != MEM_VIEW_APPLE))
			eView = MEM_VIEW_ASCII;

		iAddress = nAddress;
		for ( int iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(mem + iAddress);
			/*int iTextBackground = iBackground;
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
			{
				iTextBackground = BG_INFO_IO_BYTE;
			}
			*/
			std::string sImmediate = ColorizeSpecialChar( nImmediate, eView, iBackground );
			PrintTextCursorX( sImmediate.c_str(), rect);

			iAddress++;
		}
/*
	// Colorized Text
		iAddress = nAddress;
		for ( int iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(membank + iAddress);
			int iTextBackground = iBackground; // BG_INFO_CHAR;
//pMD->eView == MEM_VIEW_HEX
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
				iTextBackground = BG_INFO_IO_BYTE;

			std::string sImmediate = ColorizeSpecialChar( nImmediate, MEM_VIEW_APPLE, iBackground );
			PrintTextCursorX( sImmediate.c_str(), rect );

			iAddress++;
		}

		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // Hack, colorize Char background "spills over to EOL"
		PrintTextCursorX( " ", rect );
*/
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // HACK: Colorize() can "spill over" to EOL

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );

		nAddress += nMaxOpcodes;

		rect.top += nFontHeight;
	}
}

//===========================================================================
static void DrawIRQInfo(int line)
{
	if (!((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	const int nFontWidth = g_aFontConfig[FONT_INFO]._nFontWidthAvg;

	const int nameWidth = 3;	// 3 chars
	const int totalWidth = nameWidth;

	RECT rect;
	rect.top = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left = DISPLAY_IRQ_COLUMN;
	rect.right = rect.left + (totalWidth * nFontWidth);

	DebuggerSetColorBG(DebuggerGetColor(BG_IRQ_TITLE));
	DebuggerSetColorFG(DebuggerGetColor(FG_IRQ_TITLE));

	if (IsIrqAsserted())
		PrintText("IRQ", rect);
	else
		PrintText("   ", rect);
}

//===========================================================================
static void DrawVideoScannerValue(int line, int vert, int horz, bool isVisible)
{
	if (!((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	const int nFontWidth = g_aFontConfig[FONT_INFO]._nFontWidthAvg;

	const int nameWidth = 2;	// 2 chars
	const int numberWidth = 3;	// 3 chars
	const int gapWidth = 1;		// 1 space
	const int totalWidth = (nameWidth + numberWidth) * 2 + gapWidth;

	RECT rect;
	rect.top = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left = DISPLAY_VIDEO_SCANNER_COLUMN;
	rect.right = rect.left + (totalWidth * nFontWidth);

	for (int i = 0; i < 2; i++)
	{
		DebuggerSetColorBG(DebuggerGetColor(BG_VIDEOSCANNER_TITLE));
		DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_TITLE));

		const int nValue = (i == 0) ? vert : horz;

		if (i == 0) PrintText("v:", rect);
		else        PrintText("h:", rect);
		rect.left += nameWidth * nFontWidth;

		std::string sValue = StrFormat((g_videoScannerDisplayInfo.isDecimal) ? "%03u" : "%03X", nValue);

		if (!isVisible)
			DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_INVISIBLE));	// yellow
		else
			DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_VISIBLE));		// green
		PrintText(sValue.c_str(), rect);
		rect.left += (numberWidth+gapWidth) * nFontWidth;
	}
}

//===========================================================================
static void DrawVideoScannerInfo(int line)
{
	uint16_t vert, horz;
	NTSC_GetVideoVertHorzForDebugger(vert, horz);		// update video scanner's vert/horz position - needed for when in fullspeed (GH#1164)
	int v = vert, h = horz;	// use int, since 'h - 13' can go -ve

	if (g_videoScannerDisplayInfo.isHorzReal)
	{
		h -= 13;	// UTA2e ref?

		if (h < 0)
		{
			h = h + NTSC_GetCyclesPerLine();
			v = v - 1;
			if (v < 0)
				v = v + NTSC_GetVideoLines();
		}
	}

	if (g_nCumulativeCycles != g_videoScannerDisplayInfo.lastCumulativeCycles)
	{
		g_videoScannerDisplayInfo.cycleDelta = (UINT) (g_nCumulativeCycles - g_videoScannerDisplayInfo.lastCumulativeCycles);
		g_videoScannerDisplayInfo.lastCumulativeCycles = g_nCumulativeCycles;
	}

	DrawVideoScannerValue(line, v, h, NTSC_IsVisible());
	line++;

	//

	const int nFontWidth = g_aFontConfig[FONT_INFO]._nFontWidthAvg;
	const int nameWidth = 7;
	const int numberWidth = 8;
	const int totalWidth = nameWidth + numberWidth;

	RECT rect;
	rect.top = line * g_nFontHeight;
	rect.bottom = rect.top + g_nFontHeight;
	rect.left = DISPLAY_VIDEO_SCANNER_COLUMN;
	rect.right = rect.left + (totalWidth * nFontWidth);

	DebuggerSetColorBG(DebuggerGetColor(BG_VIDEOSCANNER_TITLE));
	DebuggerSetColorFG(DebuggerGetColor(FG_VIDEOSCANNER_TITLE));

	PrintText("cycles:", rect);
	rect.left += nameWidth * nFontWidth;

	UINT cycles = 0;
	if (g_videoScannerDisplayInfo.cycleMode == VideoScannerDisplayInfo::abs)
		cycles = (UINT)g_nCumulativeCycles;
	else if (g_videoScannerDisplayInfo.cycleMode == VideoScannerDisplayInfo::rel)
		cycles = g_videoScannerDisplayInfo.cycleDelta;
	else // "part"
		cycles = (UINT)g_videoScannerDisplayInfo.lastCumulativeCycles - (UINT)g_videoScannerDisplayInfo.savedCumulativeCycles;

	PrintText(StrFormat("%08X", cycles).c_str(), rect);
}

//===========================================================================
void DrawSubWindow_Info ( Update_t bUpdate, int iWindow )
{
	if (g_iWindowThis == WINDOW_CONSOLE)
		return;

	// Left Side
		int yRegs     = 0; // 12
		int yStack    = yRegs  + MAX_DISPLAY_REGS_LINES  + 0; // 0
		int yTarget   = yStack + MAX_DISPLAY_STACK_LINES - 1; // 9
		int yZeroPage = 16; // yTarget 
		int ySoft = yZeroPage + (2 * MAX_DISPLAY_ZEROPAGE_LINES) + !SOFTSWITCH_LANGCARD;
		int yBeam = ySoft - 3;

		if (bUpdate & UPDATE_VIDEOSCANNER)
			DrawVideoScannerInfo(yBeam);

		DrawIRQInfo(yBeam);

		if ((bUpdate & UPDATE_REGS) || (bUpdate & UPDATE_FLAGS))
			DrawRegisters( yRegs );

		if (bUpdate & UPDATE_STACK)
			DrawStack( yStack );

		// 2.7.0.2 Fixed: Debug build of debugger force display all CPU info window wasn't calling DrawTargets()
		bool bForceDisplayTargetPtr = DEBUG_FORCE_DISPLAY || (g_bConfigInfoTargetPointer);
		if (bForceDisplayTargetPtr || (bUpdate & UPDATE_TARGETS))
			DrawTargets( yTarget );
		
		if (bUpdate & UPDATE_ZERO_PAGE)
			DrawZeroPagePointers( yZeroPage );

		DrawSoftSwitches( ySoft );

	#if defined(SUPPORT_Z80_EMU) && defined(OUTPUT_Z80_REGS)
		DrawRegister( 19,"AF",2,*(WORD*)(membank+REG_AF));
		DrawRegister( 20,"BC",2,*(WORD*)(membank+REG_BC));
		DrawRegister( 21,"DE",2,*(WORD*)(membank+REG_DE));
		DrawRegister( 22,"HL",2,*(WORD*)(membank+REG_HL));
		DrawRegister( 23,"IX",2,*(WORD*)(membank+REG_IX));
	#endif

	// Right Side
		int yBreakpoints = 0;
		int yWatches     = yBreakpoints + MAX_BREAKPOINTS; // MAX_DISPLAY_BREAKPOINTS_LINES; // 7
		const UINT numVideoScannerInfoLines = 4;		// There used to be 2 extra watches (and each watch is 2 lines)
		int yMemory      = yWatches + numVideoScannerInfoLines + (MAX_WATCHES*2); // MAX_DISPLAY_WATCHES_LINES    ; // 14 // 2.7.0.15 Fixed: Memory Dump was over-writing watches

	//	if ((MAX_DISPLAY_BREAKPOINTS_LINES + MAX_DISPLAY_WATCHES_LINES) < 12)
	//		yWatches++;

		bool bForceDisplayBreakpoints = DEBUG_FORCE_DISPLAY || (g_nBreakpoints > 0); // 2.7.0.11 Fixed: Breakpoints and Watches no longer disappear.
		if ( bForceDisplayBreakpoints || (bUpdate & UPDATE_BREAKPOINTS))
			DrawBreakpoints( yBreakpoints );

		bool bForceDisplayWatches = DEBUG_FORCE_DISPLAY || (g_nWatches > 0); // 2.7.0.11 Fixed: Breakpoints and Watches no longer disappear.
		if ( bForceDisplayWatches || (bUpdate & UPDATE_WATCH))
			DrawWatches( yWatches );

		g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_1;

		bool bForceDisplayMemory1 = DEBUG_FORCE_DISPLAY || (g_aMemDump[0].bActive);
		if (bForceDisplayMemory1 || (bUpdate & UPDATE_MEM_DUMP))
			DrawMemory( yMemory, 0 ); // g_aMemDump[0].nAddress, g_aMemDump[0].eDevice);

		yMemory += (1 + g_nDisplayMemoryLines + 1);	// Title(1) + Lines(4) + Gap(1)
		g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_2;

		bool bForceDisplayMemory2 = DEBUG_FORCE_DISPLAY || (g_aMemDump[1].bActive);
		if (bForceDisplayMemory2 || (bUpdate & UPDATE_MEM_DUMP))
			DrawMemory( yMemory, 1 ); // g_aMemDump[1].nAddress, g_aMemDump[1].eDevice);
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

	DebuggerSetColorFG( DebuggerGetColor( FG_SOURCE ));

	const int nLines  = g_nDisasmWinHeight;

	int y = g_nDisasmWinHeight;
	int nHeight = g_nDisasmWinHeight;

	if (g_aWindowConfig[ g_iWindowThis ].bSplit) // HACK: Split Window Height is odd, so bottom window gets +1 height
		nHeight++;

	RECT rect = { 0 };
	rect.top    = (y * g_nFontHeight);
	rect.bottom = rect.top + (nHeight * g_nFontHeight);
	rect.left = 0;
	rect.right = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 7

// Draw Title
	std::string sTitle = "   Source: " + g_aSourceFileName;
	sTitle.resize(MIN(sTitle.size(), size_t(g_nConsoleDisplayWidth)));

	DebuggerSetColorBG( DebuggerGetColor( BG_SOURCE_TITLE ));
	DebuggerSetColorFG( DebuggerGetColor( FG_SOURCE_TITLE ));
	PrintText( sTitle.c_str(), rect );
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

	for ( int iLine = 0; iLine < nLines; iLine++ )
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
		DebuggerSetColorBG( DebuggerGetColor( iBackground ));
		DebuggerSetColorFG( DebuggerGetColor( iForeground ));

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

	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

// Full Screen console
//===========================================================================
void DrawWindow_Console( Update_t bUpdate )
{
	// Nothing to do, since text and draw background handled by DrawSubWindow_Console()
	// If the full screen console is only showing partial lines
	// don't erase the background
	
	//		FillRect( GetDebuggerMemDC(), &rect, g_hConsoleBrushBG );
}

//===========================================================================
void DrawWindow_Data( Update_t bUpdate )
{
	DrawSubWindow_Data( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_IO( Update_t bUpdate )
{
	DrawSubWindow_IO( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_Source( Update_t bUpdate )
{
	DrawSubWindow_Source( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindow_Symbols( Update_t bUpdate )
{
	DrawSubWindow_Symbols( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

void DrawWindow_ZeroPage( Update_t bUpdate )
{
	DrawSubWindow_ZeroPage( g_iWindowThis );
	DrawSubWindow_Info( bUpdate, g_iWindowThis );
}

//===========================================================================
void DrawWindowBackground_Main( int g_iWindowThis )
{
	RECT rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = DISPLAY_DISASM_RIGHT;
	int nTop = GetConsoleTopPixels( g_nConsoleDisplayLines - 1 );
	rect.bottom = nTop;

	// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !
	DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_1 )); // COLOR_BG_CODE
	
#if !DEBUG_FONT_NO_BACKGROUND_FILL_MAIN
	FillRect( GetDebuggerMemDC(), &rect, g_hConsoleBrushBG );
#endif
}

//===========================================================================
void DrawWindowBackground_Info( int g_iWindowThis )
{
    RECT rect;
    rect.top    = 0;
    rect.left   = DISPLAY_DISASM_RIGHT;
    rect.right  = DISPLAY_WIDTH;
	int nTop = GetConsoleTopPixels( g_nConsoleDisplayLines - 1 );
	rect.bottom = nTop;

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA

#if !DEBUG_FONT_NO_BACKGROUND_FILL_INFO
	FillRect( GetDebuggerMemDC(), &rect, g_hConsoleBrushBG );
#endif
}

//===========================================================================
void UpdateDisplay (Update_t bUpdate)
{
	static int spDrawMutex = false;
	
	if (spDrawMutex)
	{
#if DEBUG
		GetFrame().FrameMessageBox( "Already drawing!", "!", MB_OK );
#endif
	}
	spDrawMutex = true;

	// Hack: Full screen console scrolled, "erase" left over console lines
	if (g_iWindowThis == WINDOW_CONSOLE)
		bUpdate |= UPDATE_BACKGROUND;

	if (bUpdate & UPDATE_BACKGROUND)
	{
#if USE_APPLE_FONT
		SetBkMode( GetDebuggerMemDC(), OPAQUE);
		SetBkColor(GetDebuggerMemDC(), RGB(0,0,0));
#else
		SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger
#endif
	}

	SetTextAlign( GetDebuggerMemDC(), TA_TOP | TA_LEFT);

	if ((bUpdate & UPDATE_BREAKPOINTS)
//		|| (bUpdate & UPDATE_DISASM)
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
	
	switch ( g_iWindowThis )
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
			break;

		case WINDOW_SOURCE:
			DrawWindow_Source( bUpdate );
			break;

		case WINDOW_SYMBOLS:
			DrawWindow_Symbols( bUpdate );
			break;

		case WINDOW_ZEROPAGE:
			DrawWindow_ZeroPage( bUpdate );
			break;

		default:
			break;
	}

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY) || (bUpdate & UPDATE_CONSOLE_INPUT))
		DrawSubWindow_Console( bUpdate );

	StretchBltMemToFrameDC();

	spDrawMutex = false;
}

//===========================================================================
void DrawWindowBottom ( Update_t bUpdate, int iWindow )
{
	if (! g_aWindowConfig[ iWindow ].bSplit)
		return;

	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];
	g_pDisplayWindow = pWindow;

	if (pWindow->eBot == WINDOW_DATA)
		DrawWindow_Data( bUpdate ); // false
	else	
	if (pWindow->eBot == WINDOW_SOURCE)
		DrawSubWindow_Source2( bUpdate );
}

//===========================================================================
void DrawSubWindow_Code ( int iWindow )
{
	int nLines = g_nDisasmWinHeight;

//	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

	// Check if we have a bad disasm
	// BUG: This still doesn't catch all cases
	// G FB53, SPACE, PgDn * 
	// Note: DrawDisassemblyLine() has kludge.
//		DisasmCalcTopFromCurAddress( false );
	// These should be functionally equivalent.
	//	DisasmCalcTopFromCurAddress();
	//	DisasmCalcBotFromTopAddress();
#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont );
#endif

	WORD nAddress = g_nDisasmTopAddress; // g_nDisasmCurAddress;
	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		nAddress += DrawDisassemblyLine( iLine, nAddress );
	}

#if !USE_APPLE_FONT
	SelectObject( GetDebuggerMemDC(), g_aFontConfig[ FONT_INFO ]._hFont );
#endif
}
