#pragma once

// Types ____________________________________________________________
enum VideoType_e
{
	  VT_MONO_CUSTOM
	, VT_COLOR_STANDARD
	, VT_COLOR_TEXT_OPTIMIZED
	, VT_COLOR_TVEMU
	, VT_COLOR_HALF_SHIFT_DIM
	, VT_MONO_AMBER
	, VT_MONO_GREEN
	, VT_MONO_WHITE
//	, VT_MONO_AUTHENTIC
	, NUM_VIDEO_MODES
};

enum AppleFont_e
{
	// 40-Column mode is 1x Zoom (default)
	// 80-Column mode is ~0.75x Zoom (7 x 16)
	// Tiny mode is 0.5 zoom (7x8) for debugger
	APPLE_FONT_WIDTH  = 14, // in pixels
	APPLE_FONT_HEIGHT = 16, // in pixels

	// Each cell has a reserved aligned pixel area (grid spacing)
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

// Globals __________________________________________________________

extern HBITMAP g_hLogoBitmap;

extern BOOL       graphicsmode;
extern COLORREF   monochrome; // saved
extern DWORD      g_eVideoType; // saved
extern DWORD      g_uHalfScanLines; // saved
extern LPBYTE     g_pFramebufferbits;

// Prototypes _______________________________________________________

void    CreateColorMixMap();

BOOL    VideoApparentlyDirty ();
void    VideoBenchmark ();
void    VideoCheckPage (BOOL);
void    VideoChooseColor ();
void    VideoDestroy ();
void    VideoDrawLogoBitmap( HDC hDstDC );
void    VideoDisplayLogo ();
BOOL    VideoHasRefreshed ();
void    VideoInitialize ();
void    VideoRealizePalette (HDC);
void    VideoRedrawScreen ();
void    VideoRefreshScreen ();
void    VideoReinitialize ();
void    VideoResetState ();
WORD    VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles);
bool    VideoGetVbl(DWORD uExecutedCycles);
void    VideoUpdateVbl (DWORD dwCyclesThisFrame);
void    VideoUpdateFlash();
bool    VideoGetSW80COL();
DWORD   VideoGetSnapshot(SS_IO_Video* pSS);
DWORD   VideoSetSnapshot(SS_IO_Video* pSS);

typedef bool (*VideoUpdateFuncPtr_t)(int,int,int,int,int);

extern bool g_bVideoDisplayPage2;
extern bool g_VideoForceFullRedraw;

void _Video_Dirty();
void _Video_RedrawScreen( VideoUpdateFuncPtr_t update, bool bMixed = false );
void _Video_SetupBanks( bool bBank2 );
bool Update40ColCell (int x, int y, int xpixel, int ypixel, int offset);
bool Update80ColCell (int x, int y, int xpixel, int ypixel, int offset);
bool UpdateLoResCell (int x, int y, int xpixel, int ypixel, int offset);
bool UpdateDLoResCell (int x, int y, int xpixel, int ypixel, int offset);
bool UpdateHiResCell (int x, int y, int xpixel, int ypixel, int offset);
bool UpdateDHiResCell (int x, int y, int xpixel, int ypixel, int offset);

extern bool g_bDisplayPrintScreenFileName;
void Video_ResetScreenshotCounter( char *pDiskImageFileName );
enum VideoScreenShot_e
{
	SCREENSHOT_560x384 = 0,
	SCREENSHOT_280x192
};
void Video_TakeScreenShot( int iScreenShotType );

BYTE __stdcall VideoCheckMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoCheckVbl (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoSetMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
