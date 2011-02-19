#pragma once

// Types ____________________________________________________________

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	// NOTE: Used/Serialized by: g_eVideoType
	enum VideoType_e
	{
		  VT_MONO_HALFPIXEL_REAL // uses custom monochrome
		, VT_COLOR_STANDARD
		, VT_COLOR_TEXT_OPTIMIZED 
		, VT_COLOR_TVEMU          
#if _DEBUG
		, VT_COLOR_HALF_SHIFT_DIM // Michael's 80's retro look --- must >= VT_COLOR_STANDARD && <= VT_COLOR_AUTHENTIC. See: V_CreateDIBSections()
		, VT_ORG_COLOR_STANDARD
		, VT_ORG_COLOR_TEXT_OPTIMIZED
		, VT_COLOR_COLUMN_VISUALIZER
#endif
		, VT_MONO_AMBER // now half pixel
		, VT_MONO_GREEN // now half pixel
		, VT_MONO_WHITE // now half pixel
#if _DEBUG
		, VT_MONO_CUSTOM
		, VT_MONO_COLORIZE
		, VT_MONO_HALFPIXEL_COLORIZE
		, VT_MONO_HALFPIXEL_75
		, VT_MONO_HALFPIXEL_95
		, VT_MONO_HALFPIXEL_EMBOSS
		, VT_MONO_HALFPIXEL_FAKE
#endif
		, NUM_VIDEO_MODES
	};

	extern TCHAR g_aVideoChoices[];
	extern char *g_apVideoModeDesc[ NUM_VIDEO_MODES ];

	enum VideoFlag_e
	{
		VF_80COL  = 0x00000001,
		VF_DHIRES = 0x00000002,
		VF_HIRES  = 0x00000004,
		VF_MASK2  = 0x00000008,
		VF_MIXED  = 0x00000010,
		VF_PAGE2  = 0x00000020,
		VF_TEXT   = 0x00000040
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

extern BOOL       g_bGraphicsMode;
extern COLORREF   monochrome; // saved
extern DWORD      g_eVideoType; // saved
extern DWORD      g_uHalfScanLines; // saved
extern LPBYTE     g_pFramebufferbits;

extern int g_nAltCharSetOffset;
extern int g_bVideoMode; // g_bVideoMode

typedef bool (*VideoUpdateFuncPtr_t)(int,int,int,int,int);

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
VideoUpdateFuncPtr_t VideoRedrawScreen ();
VideoUpdateFuncPtr_t VideoRefreshScreen ();
void    VideoReinitialize ();
void    VideoResetState ();
WORD    VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles);
bool    VideoGetVbl(DWORD uExecutedCycles);
void    VideoUpdateFlash();
bool    VideoGetSW80COL();
DWORD   VideoGetSnapshot(SS_IO_Video* pSS);
DWORD   VideoSetSnapshot(SS_IO_Video* pSS);


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

// Win32/MSVC: __stdcall 
BYTE VideoCheckMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE VideoCheckVbl (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
BYTE VideoSetMode (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
