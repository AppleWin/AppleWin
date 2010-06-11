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

/* Description: Debugger
 *
 * Author: Copyright (C) 2006, Michael Pohoreski
 */

#include "StdAfx.h"


// NEW UI debugging
#define DEBUG_FORCE_DISPLAY 0

#if _DEBUG
	#define DEBUG_FONT_NO_BACKGROUND_CHAR      0
	#define DEBUG_FONT_NO_BACKGROUND_TEXT      0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_CON  0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_INFO 0
	#define DEBUG_FONT_NO_BACKGROUND_FILL_MAIN 0

	// no top console line
	#define DEBUG_BACKGROUND 0
#endif

//#define WATCH_ZERO_BG BG_DATA_1
#define WATCH_ZERO_BG BG_INFO

	#define DISPLAY_MEMORY_TITLE     1
//	#define DISPLAY_BREAKPOINT_TITLE 1
//	#define DISPLAY_WATCH_TITLE      1

// Public _________________________________________________________________________________________

// Font
	FontConfig_t g_aFontConfig[ NUM_FONTS  ];

// Private ________________________________________________________________________________________

// Display - Win32
//	HDC     g_hDstDC = NULL; // App Window

	HDC     g_hConsoleFontDC     = NULL;
	HBRUSH  g_hConsoleFontBrush  = NULL;
	HBITMAP g_hConsoleFontBitmap = NULL;

	HBRUSH g_hConsoleBrushFG = NULL;
	HBRUSH g_hConsoleBrushBG = NULL;

	COLORREF g_anConsoleColor[ NUM_CONSOLE_COLORS ] =
	{
		RGB(   0,   0,   0 ), // 0 000 K
		RGB( 255,  32,  32 ), // 1 001 R
		RGB(   0, 255,   0 ), // 2 010 G
		RGB( 255, 255,   0 ), // 3 011 Y
		RGB(  64,  64, 255 ), // 4 100 B
//		RGB( 255,   0, 255 ), // 5 101 M Purple/Magenta is useless
		RGB(  80, 192, 255 ),
		RGB(   0, 255, 255 ), // 6 110 C
		RGB( 255, 255, 255 ), // 7 111 W
		RGB( 255, 128,   0 ), // 8     Orange
		RGB( 128, 128, 128 )  // 9     Grey
	};

// Disassembly
	/*
		// Thought about moving MouseText to another location, say high bit, 'A' + 0x80
		// But would like to keep compatibility with existing CHARSET40
		// Since we should be able to display all apple chars 0x00 .. 0xFF with minimal processing
		// Use CONSOLE_COLOR_ESCAPE_CHAR to shift to mouse text
		* Apple Font
		    K      Mouse Text Up Arror
		    H      Mouse Text Left Arrow
		    J      Mouse Text Down Arrow
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
#if USE_APPLE_FONT
	char * g_sConfigBranchIndicatorUp   [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "^", "\x8B" }; // "`K" 0x4B
	char * g_sConfigBranchIndicatorEqual[ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "=", "\x88" }; // "`H" 0x48
	char * g_sConfigBranchIndicatorDown [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "v", "\x8A" }; // "`J" 0x4A
#else
	char * g_sConfigBranchIndicatorUp   [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "^", "\x35" };
	char * g_sConfigBranchIndicatorEqual[ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "=", "\x33" };
	char * g_sConfigBranchIndicatorDown [ NUM_DISASM_BRANCH_TYPES+1 ] = { " ", "v", "\x36" };
#endif

// Drawing
	// Width
		const int DISPLAY_WIDTH  = 560;
		// New Font = 50.5 char * 7 px/char = 353.5
		const int DISPLAY_DISASM_RIGHT   = 353 ;

#if USE_APPLE_FONT
		// Horizontal Column (pixels) of Stack & Regs
		const int INFO_COL_1 = (51 * 7); // nFontWidth
		const int DISPLAY_REGS_COLUMN    = INFO_COL_1;
		const int DISPLAY_FLAG_COLUMN    = INFO_COL_1;
		const int DISPLAY_STACK_COLUMN   = INFO_COL_1;
		const int DISPLAY_TARGETS_COLUMN = INFO_COL_1;
		const int DISPLAY_ZEROPAGE_COLUMN= INFO_COL_1;

		// Horizontal Column (pixels) of BPs, Watches & Mem
		const int INFO_COL_2 = (62 * 7); // nFontWidth
		const int DISPLAY_BP_COLUMN      = INFO_COL_2;
		const int DISPLAY_WATCHES_COLUMN = INFO_COL_2;
		const int DISPLAY_MINIMEM_COLUMN = INFO_COL_2;
#else
		const int DISPLAY_REGS_COLUMN    = SCREENSPLIT1;
		const int DISPLAY_FLAG_COLUMN    = SCREENSPLIT1; // + 63;
		const int DISPLAY_STACK_COLUMN   = SCREENSPLIT1;
		const int DISPLAY_TARGETS_COLUMN = SCREENSPLIT1;
		const int DISPLAY_ZEROPAGE_COLUMN= SCREENSPLIT1;

		const int SCREENSPLIT2 = SCREENSPLIT1 + (12 * 7); // moved left 3 chars to show B. prefix in breakpoint #, W. prefix in watch #
		const int DISPLAY_BP_COLUMN      = SCREENSPLIT2;
		const int DISPLAY_WATCHES_COLUMN = SCREENSPLIT2;
		const int DISPLAY_MINIMEM_COLUMN = SCREENSPLIT2; // nFontWidth
#endif

		int MAX_DISPLAY_REGS_LINES        = 7;
		int MAX_DISPLAY_STACK_LINES       = 8;
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

//	static HDC g_hDC = 0;


static	void SetupColorsHiLoBits ( bool bHiBit, bool bLoBit, 
			const int iBackground, const int iForeground,
			const int iColorHiBG , /*const int iColorHiFG,
			const int iColorLoBG , */const int iColorLoFG );
static	char ColorizeSpecialChar( char * sText, BYTE nData, const MemoryView_e iView,
			const int iTxtBackground  = BG_INFO     , const int iTxtForeground  = FG_DISASM_CHAR,
			const int iHighBackground = BG_INFO_CHAR, const int iHighForeground = FG_INFO_CHAR_HI,
			const int iLowBackground  = BG_INFO_CHAR, const int iLowForeground  = FG_INFO_CHAR_LO );

	char  FormatCharTxtAsci ( const BYTE b, bool * pWasAsci_ );

	void DrawSubWindow_Code ( int iWindow );
	void DrawSubWindow_IO       (Update_t bUpdate);
	void DrawSubWindow_Source1  (Update_t bUpdate);
	void DrawSubWindow_Source2  (Update_t bUpdate);
	void DrawSubWindow_Symbols  (Update_t bUpdate);
	void DrawSubWindow_ZeroPage (Update_t bUpdate);

	void DrawWindowBottom ( Update_t bUpdate, int iWindow );


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
const DWORD aROP4[ 256 ] =
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

// Font: Apple Text
//===========================================================================
void DebuggerSetColorFG( COLORREF nRGB )
{
#if USE_APPLE_FONT
	if (g_hConsoleBrushFG)
	{
		SelectObject( g_hFrameDC, GetStockObject(NULL_BRUSH) );
		DeleteObject( g_hConsoleBrushFG );
		g_hConsoleBrushFG = NULL;
	}

	g_hConsoleBrushFG = CreateSolidBrush( nRGB );
#else
	SetTextColor( g_hFrameDC, nRGB );
#endif
}

//===================================================
void DebuggerSetColorBG( COLORREF nRGB, bool bTransparent )
{
#if USE_APPLE_FONT
	if (g_hConsoleBrushBG)
	{
		SelectObject( g_hFrameDC, GetStockObject(NULL_BRUSH) );
		DeleteObject( g_hConsoleBrushBG );
		g_hConsoleBrushBG = NULL;
	}

	if (! bTransparent)
	{
		g_hConsoleBrushBG = CreateSolidBrush( nRGB );
	}
#else
	SetBkColor( g_hFrameDC, nRGB );
#endif
}

// @param glyph Specifies a native glyph from the 16x16 chars Apple Font Texture.
//===========================================================================
void PrintGlyph( const int x, const int y, const char glyph )
{	
	HDC g_hDstDC = FrameGetDC();

	int xDst = x;
	int yDst = y;

	// 16x8 chars in bitmap
	int xSrc = (glyph & 0x0F) * CONSOLE_FONT_GRID_X;
	int ySrc = (glyph >>   4) * CONSOLE_FONT_GRID_Y;

#if !DEBUG_FONT_NO_BACKGROUND_CHAR 
	// Background color
	if (g_hConsoleBrushBG)
	{
		SelectObject( g_hDstDC, g_hConsoleBrushBG );

		// Draw Background (solid pattern)
		BitBlt(
			g_hFrameDC,   // hdcDest
			xDst, yDst, // nXDest, nYDest
			CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT, // nWidth, nHeight
			g_hConsoleFontDC, // hdcSrc
			0, CONSOLE_FONT_GRID_Y * 2,  // nXSrc, nYSrc // FontTexture[2][0] = Solid (Filled) Space
			PATCOPY     // dwRop
		);
	}
#endif

//	SelectObject( g_hDstDC, GetStockBrush( WHITE_BRUSH ) );

	// http://kkow.net/etep/docs/rop.html
	//  P 1 1 1 1 0 0 0 0 (Pen/Pattern)
	//  S 1 1 0 0 1 1 0 0 (Source)
	//  D 1 0 1 0 1 0 1 0 (Destination)
	//  =================
	//    0 0 1 0 0 0 1 0 0x22 DSna
	//    1 1 1 0 1 0 1 0 0xEA DPSao

	// Black = Transparent (DC Background)
	// White = Opaque (DC Text color)

#if DEBUG_FONT_ROP
	SelectObject( g_hDstDC, g_hConsoleBrushFG );
	BitBlt(
		g_hFrameDC,
		xDst, yDst,
		DEBUG_FONT_WIDTH, DEBUG_FONT_HEIGHT,
		g_hDebugFontDC,
		xSrc, ySrc,
		aROP4[ iRop4 ]
	);
#else
	// Use inverted source as mask (AND)
	// D & ~S      ->  DSna
	BitBlt(
		g_hFrameDC,
		xDst, yDst,
		CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT,
		g_hConsoleFontDC,
		xSrc, ySrc,
		DSna
	);

	SelectObject( g_hDstDC, g_hConsoleBrushFG );

	// Use Source ask mask to make color Pattern mask (AND), then apply to dest (OR)
	// D | (P & S) ->  DPSao
	BitBlt(
		g_hFrameDC,
		xDst, yDst,
		CONSOLE_FONT_WIDTH, CONSOLE_FONT_HEIGHT,
		g_hConsoleFontDC,
		xSrc, ySrc,
		DPSao
	);
#endif

	SelectObject( g_hFrameDC, GetStockObject(NULL_BRUSH) );
}


//===========================================================================
void DebuggerPrint ( int x, int y, const char *pText )
{
	const int nLeft = x;

	char c;
	const char *p = pText;
	
	while (c = *p)
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

	if( !pText)
		return;

	while (g = (*pSrc))
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
	if (g_bDebuggerViewingAppleOutput)
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
		MessageBox( NULL, "pText = NULL!", "DrawText()", MB_OK );
#endif

	int nLen = strlen( pText );

#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillRect( g_hFrameDC, &rRect, g_hConsoleBrushBG );
#endif

	DebuggerPrint( rRect.left, rRect.top, pText );
	return nLen;
}

//===========================================================================
void PrintTextColor ( const conchar_t *pText, RECT & rRect )
{
#if !DEBUG_FONT_NO_BACKGROUND_TEXT
	FillRect( g_hFrameDC, &rRect, g_hConsoleBrushBG );
#endif

	DebuggerPrintColor( rRect.left, rRect.top, pText );
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
char ColorizeSpecialChar( char * sText, BYTE nData, const MemoryView_e iView,
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
		sprintf( sText, "%c", nChar );

#if OLD_CONSOLE_COLOR
	if (sText)
	{	
		if (ConsoleColor_IsEscapeMeta( nChar ))
			sprintf( sText, "%c%c", nChar, nChar );
		else
			sprintf( sText, "%c", nChar );
	}
#endif
	
//	if (hDC)
	{
		SetupColorsHiLoBits( bHighBit, bCtrlBit
			, iTextBG, iTextFG // FG_DISASM_CHAR   
			, iHighBG, iHighFG // BG_INFO_CHAR     
			, iCtrlBG, iCtrlFG // FG_DISASM_OPCODE 
		);
	}
	return nChar;
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

	const int MAX_BP_LEN = 16;
	char sText[16] = "Breakpoints"; // TODO: Move to BP1

#if DISPLAY_BREAKPOINT_TITLE
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
	PrintText( sText, rect );
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;
#endif

	int nBreakpointsDisplayed = 0;

	int iBreakpoint;
	for (iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++ )
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
			sprintf( sText, "B" );
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ) );
			sprintf( sText, "%X ", iBreakpoint );
			PrintTextCursorX( sText, rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ) );
//			strcpy( sText, "." );
//			PrintTextCursorX( sText, rect2 );

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
	extern COLORREF gaColorPalette[ NUM_PALETTE ];

	int iColor = R8 + iBreakpoint;
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BP_S_C ) );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_C );
	}
	DebuggerSetColorFG( nColor );
#endif

			sprintf( sText, "%04X", nAddress1 );
			PrintTextCursorX( sText, rect2 );

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
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 8)
	{
		nColor = DebuggerGetColor( BG_INFO );
		DebuggerSetColorBG( nColor );
		nColor = DebuggerGetColor( FG_DISASM_BP_S_X );
	}
	DebuggerSetColorFG( nColor );
#endif
				sprintf( sText, "%04X", nAddress2 );
				PrintTextCursorX( sText, rect2 );
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
	int nHeight = nHeight = g_aFontConfig[ FONT_CONSOLE ]._nFontHeight; // _nLineHeight; // _nFontHeight;
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

	RECT rect;
	rect.left   = (g_nConsoleInputChars + g_nConsolePromptLen) * nWidth;
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

	// Console background is drawn in DrawWindowBackground_Info
//	DrawConsoleLine( g_aConsoleInput, 0 );
	PrintText( g_aConsoleInput, rect );
}


// Disassembly ____________________________________________________________________________________


// Get the data needed to disassemble one line of opcodes
// Disassembly formatting flags returned
//	@parama sTargetValue_ indirect/indexed final value
//===========================================================================
int GetDisassemblyLine ( WORD nBaseAddress, DisasmLine_t & line_ )
//	char *sAddress_, char *sOpCodes_,
//	char *sTarget_, char *sTargetOffset_, int & nTargetOffset_,
//	char *sTargetPointer_, char *sTargetValue_,
//	char *sImmediate_, char & nImmediate_, char *sBranch_ )
{
	line_.Clear();

	int iOpcode;
	int iOpmode;
	int nOpbyte;

	iOpcode = _6502_GetOpmodeOpbyte( nBaseAddress, iOpmode, nOpbyte );

	line_.iOpcode = iOpcode;
	line_.iOpmode = iOpmode;
	line_.nOpbyte = nOpbyte;
	

#if _DEBUG
//	if (iLine != 41)
//		return nOpbytes;
#endif

	if (iOpmode == AM_M)
		line_.bTargetImmediate = true;

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NA))
		line_.bTargetIndirect = true; // ()

	if ((iOpmode >= AM_IZX) && (iOpmode <= AM_NZY))
		line_.bTargetIndexed = true; // ()

	if (((iOpmode >= AM_A) && (iOpmode <= AM_ZY)) || line_.bTargetIndirect)
		line_.bTargetValue = true; // #$

	if ((iOpmode == AM_AX) || (iOpmode == AM_ZX) || (iOpmode == AM_IZX) || (iOpmode == AM_IAX))
		line_.bTargetX = true; // ,X

	if ((iOpmode == AM_AY) || (iOpmode == AM_ZY) || (iOpmode == AM_NZY))
		line_.bTargetY = true; // ,Y

	const int nMaxOpcodes = 3;
	unsigned int nMinBytesLen = (nMaxOpcodes * (2 + g_bConfigDisasmOpcodeSpaces)); // 2 char for byte (or 3 with space)

	int bDisasmFormatFlags = 0;

	// Composite string that has the symbol or target nAddress
	WORD nTarget = 0;
	line_.nTargetOffset = 0;

//	if (g_aOpmodes[ iMode ]._sFormat[0])
	if ((iOpmode != AM_IMPLIED) &&
		(iOpmode != AM_1) &&
		(iOpmode != AM_2) &&
		(iOpmode != AM_3))
	{
		nTarget = *(LPWORD)(mem+nBaseAddress+1);
		if (nOpbyte == 2)
			nTarget &= 0xFF;

		if (iOpmode == AM_R) // Relative
		{
			line_.bTargetRelative = true;

			nTarget = nBaseAddress+2+(int)(signed char)nTarget;

			line_.nTarget = nTarget;
			sprintf( line_.sTargetValue, "%04X", nTarget & 0xFFFF );

			// Always show branch indicators
			//	if ((nBaseAddress == regs.pc) && CheckJump(nAddress))
			bDisasmFormatFlags |= DISASM_FORMAT_BRANCH;

			if (nTarget < nBaseAddress)
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorUp[ g_iConfigDisasmBranchType ] );
			}
			else
			if (nTarget > nBaseAddress)
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorDown[ g_iConfigDisasmBranchType ] );
			}
			else
			{
				sprintf( line_.sBranch, "%s", g_sConfigBranchIndicatorEqual[ g_iConfigDisasmBranchType ] );
			}
		}
		// intentional re-test AM_R ...

//		if ((iOpmode >= AM_A) && (iOpmode <= AM_NA))
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
			line_.nTarget  = nTarget;

			LPCTSTR pTarget = NULL;
			LPCTSTR pSymbol = FindSymbolFromAddress( nTarget );
			if (pSymbol)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
				pTarget = pSymbol;
			}

			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget - 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = +1; // U FA82   LDA $3F1 BREAK-1
				}
			}
			
			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget + 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_FORMAT_SYMBOL;
					bDisasmFormatFlags |= DISASM_FORMAT_OFFSET;
					pTarget = pSymbol;
					line_.nTargetOffset = -1; // U FA82 LDA $3F3 BREAK+1
				}
			}

			if (! (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL))
			{
					pTarget = FormatAddress( nTarget, nOpbyte );
			}				

//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, pTarget );
			if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
			{
				int nAbsTargetOffset =  (line_.nTargetOffset > 0) ? line_.nTargetOffset : - line_.nTargetOffset;
				sprintf( line_.sTargetOffset, "%d", nAbsTargetOffset );
			}
			sprintf( line_.sTarget, "%s", pTarget );


		// Indirect / Indexed
			int nTargetPartial;
			int nTargetPointer;
			WORD nTargetValue = 0; // de-ref
			int nTargetBytes;
			_6502_GetTargets( nBaseAddress, &nTargetPartial, &nTargetPointer, &nTargetBytes );

			if (nTargetPointer != NO_6502_TARGET)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_TARGET_POINTER;

				nTargetValue = *(LPWORD)(mem+nTargetPointer);

//				if (((iOpmode >= AM_A) && (iOpmode <= AM_NZ)) && (iOpmode != AM_R))
				// nTargetBytes refers to size of pointer, not size of value
//					sprintf( sTargetValue_, "%04X", nTargetValue ); // & 0xFFFF

				if (g_iConfigDisasmTargets & DISASM_TARGET_ADDR)
					sprintf( line_.sTargetPointer, "%04X", nTargetPointer & 0xFFFF );

				if (iOpmode != AM_NA ) // Indirect Absolute
				{
					bDisasmFormatFlags |= DISASM_FORMAT_TARGET_VALUE;
					if (g_iConfigDisasmTargets & DISASM_TARGET_VAL)
						sprintf( line_.sTargetValue, "%02X", nTargetValue & 0xFF );

					bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
					line_.nImmediate = (BYTE) nTargetValue;

					unsigned _char = FormatCharTxtCtrl( FormatCharTxtHigh( line_.nImmediate, NULL ), NULL );
					sprintf( line_.sImmediate, "%c", _char );

//					if (ConsoleColorIsEscapeMeta( nImmediate_ ))
#if OLD_CONSOLE_COLOR
					if (ConsoleColorIsEscapeMeta( _char ))
						sprintf( line_.sImmediate, "%c%c", _char, _char );
					else
						sprintf( line_.sImmediate, "%c", _char );
#endif
				}
				
//				if (iOpmode == AM_NA ) // Indirect Absolute
//					sprintf( sTargetValue_, "%04X", nTargetPointer & 0xFFFF );
//				else
// //					sprintf( sTargetValue_, "%02X", nTargetValue & 0xFF );
//					sprintf( sTargetValue_, "%04X:%02X", nTargetPointer & 0xFFFF, nTargetValue & 0xFF );
			}
		}
		else
		if (iOpmode == AM_M)
		{
//			sprintf( sTarget, g_aOpmodes[ iOpmode ]._sFormat, (unsigned)nTarget );
			sprintf( line_.sTarget, "%02X", (unsigned)nTarget );

			if (iOpmode == AM_M)
			{
				bDisasmFormatFlags |= DISASM_FORMAT_CHAR;
				line_.nImmediate = (BYTE) nTarget;
				unsigned _char = FormatCharTxtCtrl( FormatCharTxtHigh( line_.nImmediate, NULL ), NULL );

				sprintf( line_.sImmediate, "%c", _char );
#if OLD_CONSOLE_COLOR
				if (ConsoleColorIsEscapeMeta( _char ))
					sprintf( line_.sImmediate, "%c%c", _char, _char );
				else
					sprintf( line_.sImmediate, "%c", _char );
#endif
			}
		}
	}

	sprintf( line_.sAddress, "%04X", nBaseAddress );

	// Opcode Bytes
	FormatOpcodeBytes( nBaseAddress, line_ );

	int nSpaces = strlen( line_.sOpCodes );
    while (nSpaces < (int)nMinBytesLen)
	{
		strcat( line_.sOpCodes, " " );
		nSpaces++;
	}

	return bDisasmFormatFlags;
}	

//===========================================================================
void FormatOpcodeBytes ( WORD nBaseAddress, DisasmLine_t & line_ )
{
	int nOpbyte = line_.nOpbyte;

	char *pDst = line_.sOpCodes;
	for( int iByte = 0; iByte < nOpbyte; iByte++ )
	{
		BYTE nMem = (unsigned)*(mem+nBaseAddress + iByte);
		sprintf( pDst, "%02X", nMem ); // sBytes+strlen(sBytes)
		pDst += 2;

		if (g_bConfigDisasmOpcodeSpaces)
		{
			strcat( pDst, " " );
			pDst++; // 2.5.3.3 fix
		}
	}
}

// Formats Target string with bytes,words, string, etc...
//===========================================================================
void FormatNopcodeBytes ( WORD nBaseAddress, DisasmLine_t & line_ )
{
	char *pDst = line_.sTarget;
	for( int iByte = 0; iByte < line_.nOpbyte; )
	{
		BYTE nTarget8  = *(LPBYTE)(mem + nBaseAddress + iByte);
		WORD nTarget16 = *(LPWORD)(mem + nBaseAddress + iByte);
		
		switch( line_.iNoptype )
		{
			case NOP_BYTE_1:
				sprintf( pDst, "$%02X", nTarget8 ); // sBytes+strlen(sBytes)
				pDst += 3;
				iByte++;
				if( iByte < line_.nOpbyte )
				{
					*pDst++ = ',';
				}
				break;
			case NOP_WORD_1:
				sprintf( pDst, "$%04X", nTarget16 ); // sBytes+strlen(sBytes)
				pDst += 5;
				iByte+= 2;
				if( iByte < line_.nOpbyte )
				{
					*pDst++ = ',';
				}
				break;
			case NOP_STRING_APPLESOFT:
				iByte = line_.nOpbyte;
				strncpy( pDst, (const char*)(mem + nBaseAddress), iByte );
				pDst += iByte;
				*pDst = 0;
			default:
				break;
		}
//		else // 4 bytes
//		if( line_.nOpbyte == 4)
//		{
//		}		
//		else // 8 bytes
//		if( line_.nOpbyte == 8)
//		{
//		}		
	}
}


void FormatDisassemblyLine( const DisasmLine_t & line, char * sDisassembly, const int nBufferSize )
{
	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//
	// Data Disassembler
	//                              Label Directive       [Immediate]
	const char * pMnemonic = g_aOpcodes[ line.iOpcode ].sMnemonic;

	sprintf( sDisassembly, "%s:%s %s "
		, line.sAddress
		, line.sOpCodes
		, pMnemonic
	);

/*
	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		strcat( sDisassembly, "(" );
	}

	if (line.bTargetImmediate)
		strcat( sDisassembly, "#$" );

	if (line.bTargetValue)
		strcat( sDisassembly, line.sTarget );

	if (line.bTargetIndirect)
	{
		if (line.bTargetX)
	 		strcat( sDisassembly, ", X" );
		if (line.bTargetY)
 			strcat( sDisassembly, ", Y" );
	}

	if (line.bTargetIndexed || line.bTargetIndirect)
	{
		strcat( sDisassembly, ")" );
	}

	if (line.bTargetIndirect)
	{
		if (line.bTargetY)
 			strcat( sDisassembly, ", Y" );
	}
*/
	char sTarget[ 32 ];

	if (line.bTargetValue || line.bTargetRelative || line.bTargetImmediate)
	{
		if (line.bTargetRelative)
			strcpy( sTarget, line.sTargetValue );
		else
		if (line.bTargetImmediate)
		{
			strcat( sDisassembly, "#" );
			strcpy( sTarget, line.sTarget ); // sTarget
		}
		else
			sprintf( sTarget, g_aOpmodes[ line.iOpmode ].m_sFormat, line.nTarget );

		strcat( sDisassembly, "$" );
		strcat( sDisassembly, sTarget );
	}
}

//===========================================================================
WORD DrawDisassemblyLine ( int iLine, const WORD nBaseAddress )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return 0;

	int iOpcode;
	int iOpmode;
	int nOpbyte;
	DisasmLine_t line;
	LPCSTR pSymbol = NULL;
	const char * pMnemonic = NULL;

	int bDisasmFormatFlags = GetDisassemblyLine( nBaseAddress, line );
	DisasmData_t *pData = Disassembly_IsDataAddress( nBaseAddress );
	if( pData )
	{
		Disassembly_GetData( nBaseAddress, pData, line );
//		pSymbol = line.sLabel;
	}

	{
		pSymbol = FindSymbolFromAddress( nBaseAddress );
//		strcpy( line.sLabel, pSymbol );
	}

	iOpcode = line.iOpcode;	
	iOpmode = line.iOpmode;
	nOpbyte = line.nOpbyte;

	// Data Disassembler
	//
	// sAddress, sOpcodes, sTarget, sTargetOffset, nTargetOffset, sTargetPointer, sTargetValue, sImmediate, nImmediate, sBranch );
	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
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
//	{ 6, 16, 26, 41, 46, 49 }; // 6, 17, 26, 40, 46
#if USE_APPLE_FONT
//	{ 5, 14, 20, 40, 46, 49 };
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
	const int OPCODE_TO_LABEL_SPACE = static_cast<int>( aTabs[ TS_INSTRUCTION ] - aTabs[ TS_LABEL ] );

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

#if USE_APPLE_FONT
	const int DISASM_SYMBOL_LEN = 12;
#else
	const int DISASM_SYMBOL_LEN = 9;
#endif

	HDC dc = g_hFrameDC;
	if (dc)
	{
		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight; // _nFontHeight; // g_nFontHeight

		RECT linerect;
		linerect.left   = 0;
		linerect.top    = iLine * nFontHeight;
		linerect.right  = DISPLAY_DISASM_RIGHT;
		linerect.bottom = linerect.top + nFontHeight;

//		BOOL bp = g_nBreakpoints && CheckBreakpoint(nBaseAddress,nBaseAddress == regs.pc);

		bool bBreakpointActive;
		bool bBreakpointEnable;
		GetBreakpointInfo( nBaseAddress, bBreakpointActive, bBreakpointEnable );
		bool bAddressAtPC = (nBaseAddress == regs.pc);
		bool bAddressIsBookmark = Bookmark_Find( nBaseAddress );

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

		if (bAddressIsBookmark)
		{
			DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_BOOKMARK ) );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_BOOKMARK ) );
		}
		else
		{
			DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
		}
		
	// Address
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ) );
//		else
//		{
//			DebuggerSetColorBG( dc, DebuggerGetColor( FG_DISASM_BOOKMARK ) ); // swapped
//			DebuggerSetColorFG( dc, DebuggerGetColor( BG_DISASM_BOOKMARK ) ); // swapped
//		}		

		if( g_bConfigDisasmAddressView )
		{
			PrintTextCursorX( (LPCTSTR) line.sAddress, linerect );
		}

		if (bAddressIsBookmark)
		{
			DebuggerSetColorBG( DebuggerGetColor( iBackground ) );
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );
		}
		
	// Address Seperator		
		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );

		if (g_bConfigDisasmAddressColon)
			PrintTextCursorX( ":", linerect );
		else
			PrintTextCursorX( " ", linerect ); // bugfix, not showing "addr:" doesn't alternate color lines

	// Opcodes
		linerect.left = (int) aTabs[ TS_OPCODE ];

		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );
//		PrintTextCursorX( " ", linerect );

		if (g_bConfigDisasmOpcodesView)
			PrintTextCursorX( (LPCTSTR) line.sOpCodes, linerect );
//		PrintTextCursorX( "  ", linerect );

	// Label
		linerect.left = (int) aTabs[ TS_LABEL ];

		LPCSTR pSymbol = FindSymbolFromAddress( nBaseAddress );
		if (pSymbol)
		{
			if (! bCursorLine)
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
			PrintTextCursorX( pSymbol, linerect );
		}	
//		linerect.left += (g_nFontWidthAvg * DISASM_SYMBOL_LEN);
//		PrintTextCursorX( " ", linerect );

	// Instruction / Mnemonic
		linerect.left = (int) aTabs[ TS_INSTRUCTION ];

		if (! bCursorLine)
			DebuggerSetColorFG( DebuggerGetColor( iForeground ) );

		if( pData )
		{
//			pMnemonic = g_aAssemblerDirectives[ line.iNopcode ].sMnemonic;
			pMnemonic = line.sMnemonic;
		}
		else
		{
			pMnemonic = g_aOpcodes[ iOpcode ].sMnemonic;
		}

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
		if (*pTarget == '$')
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
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
			}
			else
			{
				if (iOpmode == AM_M)
//				if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
				{
					DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPCODE ) );
				}
				else	
				{
					DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_TARGET ) );
				}
			}
		}
		PrintTextCursorX( pTarget, linerect );
//		PrintTextCursorX( " ", linerect );

		if( pData )
		{
			return nOpbyte;
		}

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
		// Inside Parenthesis = Indirect Target Regs
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

	// Memory Pointer and Value
		if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER) // (bTargetValue)
		{
			linerect.left = (int) aTabs[ TS_IMMEDIATE ]; // TS_IMMEDIATE ];

//			PrintTextCursorX( "  ", linerect );

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

	// Immediate Char
		if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
		{
			linerect.left = (int) aTabs[ TS_CHAR ]; // TS_IMMEDIATE ];

			if (! bCursorLine)
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
			}

//			if (! (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER))
//				PrintTextCursorX( "'", linerect );

			if (! bCursorLine)
			{
				ColorizeSpecialChar( NULL, line.nImmediate, MEM_VIEW_ASCII, iBackground );
//					iBackground, FG_INFO_CHAR_HI, FG_DISASM_CHAR, FG_INFO_CHAR_LO );
			}
			PrintTextCursorX( line.sImmediate, linerect );

			DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // Hack: Colorize can "color bleed to EOL"
			if (! bCursorLine)
			{
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
			}

//			if (! (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER))
//				PrintTextCursorX( "'", linerect );
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
				SelectObject( g_hFrameDC, g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont );  // g_hFontWebDings
#endif

//			PrintTextColor( sBranch, linerect );
			PrintText( line.sBranch, linerect );

#if !USE_APPLE_FONT
			if (g_iConfigDisasmBranchType)
				SelectObject( dc, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm
#endif
		}
	}

	return nOpbyte;
}


// Optionally copy the flags to pText_
//===========================================================================
void DrawFlags ( int line, WORD nRegFlags, LPTSTR pFlagNames_)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	char sFlagNames[ _6502_NUM_FLAGS+1 ] = ""; // = "NVRBDIZC"; // copy from g_aFlagNames
	char sText[4] = "?";
	RECT  rect;

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

	if (g_hFrameDC)
	{
		rect.top    = line * g_nFontHeight;
		rect.bottom = rect.top + g_nFontHeight;
		rect.left   = DISPLAY_FLAG_COLUMN;
		rect.right  = rect.left + (10 * nFontWidth);

		DebuggerSetColorBG( DebuggerGetColor( BG_DATA_1 )); // BG_INFO
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
		PrintText( "P ", rect );

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;

		sprintf( sText, "%02X", nRegFlags );

		DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
		PrintText( sText, rect );

		rect.top    -= g_nFontHeight;
		rect.bottom -= g_nFontHeight;
		sText[1] = 0;

		rect.left += ((2 + _6502_NUM_FLAGS) * nSpacerWidth);
		rect.right = rect.left + nFontWidth;
	}

	int iFlag = 0;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		iFlag = (_6502_NUM_FLAGS - nFlag - 1);

		bool bSet = (nRegFlags & 1);
		if (g_hFrameDC)
		{
			sText[0] = g_aBreakpointSource[ BP_SRC_FLAG_C + iFlag ][0];
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
			PrintText( sText, rect );

			// Print Binary value
			rect.top    += g_nFontHeight;
			rect.bottom += g_nFontHeight;
			DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // 
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));

			sText[0] = '0' + static_cast<int>(bSet);
			PrintText( sText, rect );
			rect.top    -= g_nFontHeight;
			rect.bottom -= g_nFontHeight;
		}

		if (pFlagNames_)
		{
			if (! bSet) //(nFlags & 1))
			{
				sFlagNames[nFlag] = '.';
			}
			else
			{
				sFlagNames[nFlag] = g_aBreakpointSource[ BP_SRC_FLAG_C + iFlag ][0]; 
			}
		}

		nRegFlags >>= 1;
	}

	if (pFlagNames_)
		strcpy(pFlagNames_,sFlagNames);
/*
	if (g_hFrameDC)
	{
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
		rect.left   = DISPLAY_FLAG_COLUMN;
		rect.right  = rect.left + (10 * nFontWidth);

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
		PrintTextCursorX( "P ", rect );


		DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_REG ));
		PrintTextCursorX( "P ", rect );

		rect.left += (_6502_NUM_FLAGS * nSpacerWidth);
		rect.right = rect.left + nFontWidth;
	}
*/
}

//===========================================================================
void DrawMemory ( int line, int iMemDump )
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
	rect.left   = DISPLAY_MINIMEM_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	RECT rect2;
	rect2 = rect;

	const int MAX_MEM_VIEW_TXT = 16;
	char sText[ MAX_MEM_VIEW_TXT * 2 ];
	char sData[ MAX_MEM_VIEW_TXT * 2 ];

	char sType   [ 4 ] = "Mem";
	char sAddress[ 8 ] = "";

	int iForeground = FG_INFO_OPCODE;
	int iBackground = BG_INFO;

#if DISPLAY_MEMORY_TITLE
	if (eDevice == DEV_SY6522)
	{
//		sprintf(sData,"Mem at SY#%d", nAddr);
		sprintf( sAddress,"SY#%d", nAddr );
	}
	else if(eDevice == DEV_AY8910)
	{
//		sprintf(sData,"Mem at AY#%d", nAddr);
		sprintf( sAddress,"AY#%d", nAddr );
	}
	else
	{
		sprintf( sAddress,"%04X",(unsigned)nAddr);

		if (iView == MEM_VIEW_HEX)
			sprintf( sType, "HEX" );
		else
		if (iView == MEM_VIEW_ASCII)
			sprintf( sType, "ASCII" );
		else
			sprintf( sType, "TEXT" );
	}

	rect2 = rect;	
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
	PrintTextCursorX( sType, rect2 );

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
	PrintTextCursorX( " at ", rect2 );

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
	PrintTextCursorY( sAddress, rect2 );
#endif

	rect.top    = rect2.top;
	rect.bottom = rect2.bottom;

	sData[0] = 0;

	WORD iAddress = nAddr;

	int nLines = g_nDisplayMemoryLines;
	int nCols = 4;

	if (iView != MEM_VIEW_HEX)
	{
		nCols = MAX_MEM_VIEW_TXT;
	}

	if( (eDevice == DEV_SY6522) || (eDevice == DEV_AY8910) )
	{
		iAddress = 0;
		nCols = 6;
	}

	rect.right = DISPLAY_WIDTH - 1;

	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));

	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		rect2 = rect;

		if (iView == MEM_VIEW_HEX)
		{
			sprintf( sText, "%04X", iAddress );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );
		}

		for (int iCol = 0; iCol < nCols; iCol++)
		{
			bool bHiBit = false;
			bool bLoBit = false;

			DebuggerSetColorBG( DebuggerGetColor( iBackground ));
			DebuggerSetColorFG( DebuggerGetColor( iForeground ));

// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
//			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
//			{
//				sprintf( sText, "IO " );
//			}
//			else
			if (eDevice == DEV_SY6522)
			{
				sprintf( sText, "%02X", (unsigned) ((BYTE*)&SS_MB.Unit[nAddr & 1].RegsSY6522)[iAddress] );
				if (iCol & 1)
					DebuggerSetColorFG( DebuggerGetColor( iForeground ));
				else
					DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			}
			else
			if (eDevice == DEV_AY8910)
			{
				sprintf( sText, "%02X", (unsigned)SS_MB.Unit[nAddr & 1].RegsAY8910[iAddress] );
				if (iCol & 1)
					DebuggerSetColorFG( DebuggerGetColor( iForeground ));
				else
					DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
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
						DebuggerSetColorFG( DebuggerGetColor( FG_INFO_IO_BYTE ));
					}
					sprintf(sText, "%02X ", nData );
				}
				else
				{
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
						iBackground = BG_INFO_IO_BYTE;

					ColorizeSpecialChar( sText, nData, iView, iBackground );
				}
			}
			int nChars = PrintTextCursorX( sText, rect2 ); // PrintTextCursorX()
			iAddress++;
		}
		// Windows HACK: Bugfix: Rest of line is still background color
//		DebuggerSetColorBG(  hDC, DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA
//		DebuggerSetColorFG(hDC, DebuggerGetColor( FG_INFO_TITLE )); //COLOR_STATIC
//		PrintTextCursorX( " ", rect2 );

		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
		sData[0] = 0;
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

	unsigned int nData = nValue;
	int nOffset = 6;

	char sValue[8];

	if (PARAM_REG_SP == iSource)
	{
		WORD nStackDepth = _6502_STACK_END - nValue;
		sprintf( sValue, "%02X", nStackDepth );
		int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
		rect.left += (2 * nFontWidth) + (nFontWidth >> 1); // 2.5 looks a tad nicer then 2

		// ## = Stack Depth (in bytes)
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR )); // FG_INFO_OPCODE, FG_INFO_TITLE
		PrintText( sValue, rect );
	}

	if (nBytes == 2)
	{
		sprintf(sValue,"%04X", nData);
	}
	else
	{
		rect.left  = DISPLAY_REGS_COLUMN + (3 * nFontWidth);
//		rect.right = SCREENSPLIT2;

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX

		ColorizeSpecialChar( sValue, nData, MEM_VIEW_ASCII ); // MEM_VIEW_APPLE for inverse background little hard on the eyes

		DebuggerSetColorBG( DebuggerGetColor( iBackground ));
		PrintTextCursorX( sValue, rect ); // PrintTextCursorX()

		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
		PrintTextCursorX( "'", rect ); // PrintTextCursorX()

		sprintf(sValue,"  %02X", nData );
	}

	// Needs to be far enough over, since 4 chars of ZeroPage symbol also calls us
	rect.left  = DISPLAY_REGS_COLUMN + (nOffset * nFontWidth);

	if ((PARAM_REG_PC == iSource) || (PARAM_REG_SP == iSource)) // Stack Pointer is target address, but doesn't look as good.
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
	}
	else
	{
		DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // FG_DISASM_OPCODE
	}
	PrintText( sValue, rect );
}


//===========================================================================
void DrawSourceLine( int iSourceLine, RECT &rect )
{
	char sLine[ CONSOLE_WIDTH ];
	ZeroMemory( sLine, CONSOLE_WIDTH );

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
#if DEBUG_FORCE_DISPLAY
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

		char sText[8] = "";
		if (nAddress <= _6502_STACK_END)
		{
			sprintf( sText,"%04X: ", nAddress );
			PrintTextCursorX( sText, rect );
		}

		if (nAddress <= _6502_STACK_END)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // COLOR_FG_DATA_TEXT
			sprintf(sText, "  %02X",(unsigned)*(LPBYTE)(mem+nAddress));
			PrintTextCursorX( sText, rect );
		}
		iStack++;
	}
}


//===========================================================================
void DrawTargets ( int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int aTarget[2];
	_6502_GetTargets( regs.pc, &aTarget[0],&aTarget[1], NULL );

	RECT rect;
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
	
	int iAddress = 2;
	while (iAddress--)
	{
		// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
//		if ((aTarget[iAddress] >= _6502_IO_BEGIN) && (aTarget[iAddress] <= _6502_IO_END))
//			aTarget[iAddress] = NO_6502_TARGET;

		char sAddress[8] = "-none-";
		char sData[8]    = "";

#if DEBUG_FORCE_DISPLAY
		if (aTarget[iAddress] == NO_6502_TARGET)
			aTarget[iAddress] = 0;
#endif
		if (aTarget[iAddress] != NO_6502_TARGET)
		{
			sprintf(sAddress,"%04X",aTarget[iAddress]);
			if (iAddress)
				sprintf(sData,"%02X",*(LPBYTE)(mem+aTarget[iAddress]));
			else
				sprintf(sData,"%04X",*(LPWORD)(mem+aTarget[iAddress]));
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
		PrintText( sAddress, rect );

		rect.left  = nColumn;
		rect.right = rect.left + (10 * nFontWidth); // SCREENSPLIT2

		if (iAddress == 0)
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS )); // Temp Target
		else
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE )); // Target Bytes

		PrintText( sData, rect );
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

	char sText[16] = "Watches";

	DebuggerSetColorBG( DebuggerGetColor( WATCH_ZERO_BG )); // BG_INFO

#if DISPLAY_WATCH_TITLE
	DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ));
	PrintTextCursorY( sText, rect );
#endif

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

//			DebuggerSetColorBG( DebuggerGetColor( BG_INFO ));
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "W", rect2 );

			sprintf( sText, "%X ",iWatch );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( sText, rect2 );
			
//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( ".", rect2 );

			sprintf( sText,"%04X", g_aWatches[iWatch].nAddress );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			BYTE nTarget8 = (unsigned)*(LPBYTE)(mem+g_aWatches[iWatch].nAddress);
			sprintf(sText,"%02X", nTarget8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );

			WORD nTarget16 = (unsigned)*(LPWORD)(mem+g_aWatches[iWatch].nAddress);
			sprintf( sText," %04X", nTarget16 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			BYTE nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16);
			sprintf(sText,"%02X", nValue8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );
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

	DebuggerSetColorBG( DebuggerGetColor( WATCH_ZERO_BG )); // BG_INFO

	const int nMaxSymbolLen = 7;
	char sText[nMaxSymbolLen+1] = "";

	for(int iZP = 0; iZP < MAX_ZEROPAGE_POINTERS; iZP++)
	{
		RECT rect2 = rect;

		Breakpoint_t *pZP = &g_aZeroPagePointers[iZP];
		bool bEnabled = pZP->bEnabled;

#if DEBUG_FORCE_DISPLAY
		bEnabled = true;
#endif
		if (bEnabled)
		{
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_TITLE ) );
			PrintTextCursorX( "Z", rect2 );

			sprintf( sText, "%X ", iZP );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_BULLET ));
			PrintTextCursorX( sText, rect2 );

//			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
//			PrintTextCursorX( " ", rect2 );

			BYTE nZPAddr1 = (g_aZeroPagePointers[iZP].nAddress  ) & 0xFF; // +MJP missig: "& 0xFF", or "(BYTE) ..."
			BYTE nZPAddr2 = (g_aZeroPagePointers[iZP].nAddress+1) & 0xFF;

			// Get nZPAddr1 last (for when neither symbol is not found - GetSymbol() return ptr to static buffer)
			const char* pSymbol2 = GetSymbol(nZPAddr2, 2);		// 2:8-bit value (if symbol not found)
			const char* pSymbol1 = GetSymbol(nZPAddr1, 2);		// 2:8-bit value (if symbol not found)

			int nLen1 = strlen( pSymbol1 );
			int nLen2 = strlen( pSymbol2 );


//			if ((nLen1 == 1) && (nLen2 == 1))
//				sprintf( sText, "%s%s", pszSymbol1, pszSymbol2);

			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));

			int x;
			for( x = 0; x < nMaxSymbolLen; x++ )
			{
				sText[ x ] = CHAR_SPACE;
			}
			sText[nMaxSymbolLen] = 0;
			
			if ((nLen1) && (pSymbol1[0] == '$'))
			{
//				sprintf( sText, "%s%s", pSymbol1 );
//				sprintf( sText, "%04X", nZPAddr1 );
			}
			else
			if ((nLen2) && (pSymbol2[0] == '$'))
			{
//				sprintf( sText, "%s%s", pSymbol2 );
//				sprintf( sText, "%04X", nZPAddr2 );
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			}
			else
			{
				int nMin = min( nLen1, nMaxSymbolLen );
				memcpy(sText, pSymbol1, nMin);
				DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_SYMBOL ) );
			}
//			DrawRegister( line+iZP, szZP, 2, nTarget16);
//			PrintText( szZP, rect2 );
			PrintText( sText, rect2);

			rect2.left    = rect.left;
			rect2.top    += g_nFontHeight;
			rect2.bottom += g_nFontHeight;

			sprintf( sText, "%02X", nZPAddr1 );
			DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			WORD nTarget16 = (WORD)mem[ nZPAddr1 ] | ((WORD)mem[ nZPAddr2 ]<< 8);
			sprintf( sText, "%04X", nTarget16 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_ADDRESS ));
			PrintTextCursorX( sText, rect2 );

			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPERATOR ));
			PrintTextCursorX( ":", rect2 );

			BYTE nValue8 = (unsigned)*(LPBYTE)(mem + nTarget16);
			sprintf(sText, "%02X", nValue8 );
			DebuggerSetColorFG( DebuggerGetColor( FG_INFO_OPCODE ));
			PrintTextCursorX( sText, rect2 );
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
	SelectObject( g_hFrameDC, g_aFontConfig[ FONT_CONSOLE ]._hFont );
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
//		DrawConsoleInput(); // g_hFrameDC );
	}
}	

//===========================================================================
void DrawSubWindow_Data (Update_t bUpdate)
{
//	HDC hDC = g_hDC;
	int iBackground;	

	const int nMaxOpcodes = WINDOW_DATA_BYTES_PER_LINE;
	char  sAddress  [ 5];

	assert( CONSOLE_WIDTH > WINDOW_DATA_BYTES_PER_LINE );

	char sOpcodes  [ CONSOLE_WIDTH ] = "";
	char sImmediate[ 4 ]; // 'c'

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
		sprintf( sAddress, "%04X", iAddress );

		sOpcodes[0] = 0;
		for ( iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nData = (unsigned)*(LPBYTE)(mem + iAddress + iByte);
			sprintf( &sOpcodes[ iByte * 3 ], "%02X ", nData );
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
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) );

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_ADDRESS ) );
		PrintTextCursorX( (LPCTSTR) sAddress, rect );

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			PrintTextCursorX( ":", rect );

		rect.left = X_OPCODE;

		DebuggerSetColorFG( DebuggerGetColor( FG_DATA_BYTE ) );
		PrintTextCursorX( (LPCTSTR) sOpcodes, rect );

		rect.left = X_CHAR;

	// Seperator
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );


	// Plain Text
		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_CHAR ) );

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

			ColorizeSpecialChar( sImmediate, (BYTE) nImmediate, eView, iBackground );
			PrintTextCursorX( (LPCSTR) sImmediate, rect );

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
			PrintTextCursorX( (LPCSTR) sImmediate, rect );

			iAddress++;
		}

		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // Hack, colorize Char background "spills over to EOL"
		PrintTextCursorX( " ", rect );
*/
		DebuggerSetColorBG( DebuggerGetColor( iBackground ) ); // HACK: Colorize() can "spill over" to EOL

		DebuggerSetColorFG( DebuggerGetColor( FG_DISASM_OPERATOR ));
		PrintTextCursorX( "  |  ", rect );

		nAddress += nMaxOpcodes;
	}
}


// DrawRegisters();
//===========================================================================
void DrawSubWindow_Info( int iWindow )
{
	if (g_iWindowThis == WINDOW_CONSOLE)
		return;

	const char **sReg = g_aBreakpointSource;

	int yRegs     = 0; // 12
	int yStack    = yRegs  + MAX_DISPLAY_REGS_LINES  + 0; // 0
	int yTarget   = yStack + MAX_DISPLAY_STACK_LINES - 1; // 9
	int yZeroPage = 16; // 19

	DrawRegister( yRegs++, sReg[ BP_SRC_REG_A ] , 1, regs.a , PARAM_REG_A  );
	DrawRegister( yRegs++, sReg[ BP_SRC_REG_X ] , 1, regs.x , PARAM_REG_X  );
	DrawRegister( yRegs++, sReg[ BP_SRC_REG_Y ] , 1, regs.y , PARAM_REG_Y  );
	DrawRegister( yRegs++, sReg[ BP_SRC_REG_PC] , 2, regs.pc, PARAM_REG_PC );
	DrawFlags   ( yRegs  , regs.ps, NULL);
	yRegs += 2;
	DrawRegister( yRegs++, sReg[ BP_SRC_REG_S ] , 2, regs.sp, PARAM_REG_SP );

	DrawStack( yStack );

	if (g_bConfigInfoTargetPointer)
	{
		DrawTargets( yTarget );
	}
	
	DrawZeroPagePointers( yZeroPage );

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
	int yMemory      = yWatches     + MAX_WATCHES    ; // MAX_DISPLAY_WATCHES_LINES    ; // 14

//	if ((MAX_DISPLAY_BREAKPOINTS_LINES + MAX_DISPLAY_WATCHES_LINES) < 12)
//		yWatches++;

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nBreakpoints)
#endif
		DrawBreakpoints( yBreakpoints );

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nWatches)
#endif
		DrawWatches( yWatches );

	g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_1;

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[0].bActive)
#endif
		DrawMemory( yMemory, 0 ); // g_aMemDump[0].nAddress, g_aMemDump[0].eDevice);

	yMemory += (g_nDisplayMemoryLines + 1);
	g_nDisplayMemoryLines = MAX_DISPLAY_MEMORY_LINES_2;

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[1].bActive)
#endif
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
	char sTitle[ CONSOLE_WIDTH ];
	char sText [ CONSOLE_WIDTH ];
	strcpy ( sTitle, "   Source: " );
	strncpy( sText , g_aSourceFileName, g_nConsoleDisplayWidth - strlen( sTitle ) - 1 );
	strcat ( sTitle, sText );

	DebuggerSetColorBG( DebuggerGetColor( BG_SOURCE_TITLE ));
	DebuggerSetColorFG( DebuggerGetColor( FG_SOURCE_TITLE ));
	PrintText( sTitle, rect );
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

	DrawSubWindow_Info( g_iWindowThis );
}

// Full Screen console
//===========================================================================
void DrawWindow_Console( Update_t bUpdate )
{
	// Nothing to do, since text and draw background handled by DrawSubWindow_Console()
	// If the full screen console is only showing partial lines
	// don't erase the background
	
	//		FillRect( g_hFrameDC, &rect, g_hConsoleBrushBG );
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
	RECT rect;
	rect.left   = 0;
	rect.top    = 0;
	rect.right  = DISPLAY_DISASM_RIGHT;
	int nTop = GetConsoleTopPixels( g_nConsoleDisplayLines - 1 );
	rect.bottom = nTop; // DISPLAY_HEIGHT

	// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !
	DebuggerSetColorBG( DebuggerGetColor( BG_DISASM_1 )); // COLOR_BG_CODE
	
#if !DEBUG_FONT_NO_BACKGROUND_FILL_MAIN
	FillRect( g_hFrameDC, &rect, g_hConsoleBrushBG );
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
	rect.bottom = nTop; // DISPLAY_HEIGHT

	DebuggerSetColorBG( DebuggerGetColor( BG_INFO )); // COLOR_BG_DATA

#if !DEBUG_FONT_NO_BACKGROUND_FILL_INFO
	FillRect( g_hFrameDC, &rect, g_hConsoleBrushBG );
#endif
}


//===========================================================================
void UpdateDisplay (Update_t bUpdate)
{
	static int spDrawMutex = false;
	
	if (spDrawMutex)
	{
#if DEBUG
		MessageBox( g_hFrameWindow, "Already drawing!", "!", MB_OK );
#endif
	}
	spDrawMutex = true;

	FrameGetDC();

	// Hack: Full screen console scrolled, "erase" left over console lines
	if (g_iWindowThis == WINDOW_CONSOLE)
		bUpdate |= UPDATE_BACKGROUND;

	if (bUpdate & UPDATE_BACKGROUND)
	{
#if USE_APPLE_FONT
		//VideoDrawLogoBitmap( g_hFrameDC );	// TC: Remove purple-flash after every single-step

		SetBkMode( g_hFrameDC, OPAQUE);
		SetBkColor(g_hFrameDC, RGB(0,0,0));
#else
		SelectObject( g_hFrameDC, g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger
#endif
	}

	SetTextAlign( g_hFrameDC, TA_TOP | TA_LEFT);

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

	spDrawMutex = false;
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
#if !USE_APPLE_FONT
	SelectObject( g_hFrameDC, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont );
#endif

	WORD nAddress = g_nDisasmTopAddress; // g_nDisasmCurAddress;
	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		nAddress += DrawDisassemblyLine( iLine, nAddress );
	}

#if !USE_APPLE_FONT
	SelectObject( g_hFrameDC, g_aFontConfig[ FONT_INFO ]._hFont );
#endif
}
