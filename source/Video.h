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
		, VT_MONO_AMBER // now half pixel
		, VT_MONO_GREEN // now half pixel
		, VT_MONO_WHITE // now half pixel
		, NUM_VIDEO_MODES
	};

	extern TCHAR g_aVideoChoices[];
	extern char *g_apVideoModeDesc[ NUM_VIDEO_MODES ];

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

extern COLORREF   monochrome; // saved
extern DWORD      g_eVideoType; // saved
extern DWORD      g_uHalfScanLines; // saved
extern LPBYTE     g_pFramebufferbits;

typedef bool (*VideoUpdateFuncPtr_t)(int,int,int,int,int);

// Prototypes _______________________________________________________

void    CreateColorMixMap();

BOOL    VideoApparentlyDirty ();
void    VideoBenchmark ();
void    VideoChooseColor ();
void    VideoDestroy ();
void    VideoDrawLogoBitmap(HDC hDstDC, int xoff, int yoff, int srcw, int srch, int scale);
void    VideoDisplayLogo ();
void    VideoInitialize ();
void    VideoRealizePalette (HDC);
VideoUpdateFuncPtr_t VideoRedrawScreen (UINT);
VideoUpdateFuncPtr_t VideoRedrawScreen ();
VideoUpdateFuncPtr_t VideoRefreshScreen ();
void    VideoReinitialize ();
void    VideoResetState ();
WORD    VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles);
bool    VideoGetVbl(DWORD uExecutedCycles);
void    VideoEndOfVideoFrame(void);

bool    VideoGetSW80COL(void);
bool    VideoGetSWDHIRES(void);
bool    VideoGetSWHIRES(void);
bool    VideoGetSW80STORE(void);
bool    VideoGetSWMIXED(void);
bool    VideoGetSWPAGE2(void);
bool    VideoGetSWTEXT(void);
bool    VideoGetSWAltCharSet(void);

void    VideoSetForceFullRedraw(void);

void    VideoSetSnapshot_v1(const UINT AltCharSet, const UINT VideoMode);
void    VideoSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    VideoLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
void    VideoGetSnapshot(struct SS_IO_Video_v2& Video);
void    VideoSetSnapshot(const struct SS_IO_Video_v2& Video);

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
extern bool g_bShowPrintScreenWarningDialog;

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

void Config_Load_Video(void);
void Config_Save_Video(void);
