#pragma once

enum VIDEOTYPE
{
	  VT_MONO_CUSTOM
	, VT_COLOR_STANDARD
	, VT_COLOR_TEXT_OPTIMIZED
	, VT_COLOR_TVEMU
	, VT_COLOR_HALF_SHIFT_DIM
	, VT_MONO_AMBER
	, VT_MONO_GREEN
	, VT_MONO_WHITE
	, VT_NUM_MODES
};

extern BOOL       graphicsmode;
extern COLORREF   monochrome;
extern DWORD      videotype;

enum AppleFont_e
{
	// 40-Column mode is 2x Zoom (default)
	// 80-Column mode is 1.5x Zoom (7 x 16)
	// Tiny mode is 1x zoom (7x8) for debugger
	APPLE_FONT_WIDTH  = 14, // in pixels
	APPLE_FONT_HEIGHT = 16, // in pixels

	// Each cell has a reserved aligned pixel area
	APPLE_FONT_CELL_WIDTH  = 16,
	APPLE_FONT_CELL_HEIGHT = 16,

	// The bitmap contains 3 regions
	// Each region is 256x256 pixels = 16x16 chars
	APPLE_FONT_X_REGIONSIZE = 256, // in pixelx
	APPLE_FONT_Y_REGIONSIZE = 256, // in pixels

	// Starting Y offsets (pixels) for the regions
	APPLE_FONT_Y_APPLE_2PLUS =   0, // ][+
	APPLE_FONT_Y_APPLE_80COL = 256, // //e (inc. Mouse Text)
	APPLE_FONT_Y_APPLE_40COL = 512, // ][
};

void    CreateColorMixMap();

BOOL    VideoApparentlyDirty ();
void    VideoBenchmark ();
void    VideoCheckPage (BOOL);
void    VideoChooseColor ();
void    VideoDestroy ();
void    VideoDisplayLogo ();
BOOL    VideoHasRefreshed ();
void    VideoInitialize ();
void    VideoRealizePalette (HDC);
void    VideoRedrawScreen ();
void    VideoRefreshScreen ();
void    VideoReinitialize ();
void    VideoResetState ();
WORD    VideoGetScannerAddress(bool* pbVblBar_OUT = NULL);
void    VideoUpdateVbl (DWORD dwCyclesThisFrame);
void    VideoUpdateFlash();
bool    VideoGetSW80COL();
DWORD   VideoGetSnapshot(SS_IO_Video* pSS);
DWORD   VideoSetSnapshot(SS_IO_Video* pSS);

BYTE __stdcall VideoCheckMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoCheckVbl (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoSetMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
