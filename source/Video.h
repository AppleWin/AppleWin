#pragma once

// Types ____________________________________________________________

	// NOTE: KEEP IN SYNC: VideoType_e g_aVideoChoices g_apVideoModeDesc
	// NOTE: Used/Serialized by: g_eVideoType
	enum VideoType_e
	{
		  VT_MONO_CUSTOM
		, VT_COLOR_MONITOR_RGB		// Color rendering from AppleWin 1.25 (GH#357)
		, VT_COLOR_MONITOR_NTSC
		, VT_COLOR_TV
		, VT_MONO_TV
		, VT_MONO_AMBER
		, VT_MONO_GREEN
		, VT_MONO_WHITE
		, NUM_VIDEO_MODES
		, VT_DEFAULT = VT_COLOR_TV
	};

	extern TCHAR g_aVideoChoices[];
	extern char *g_apVideoModeDesc[ NUM_VIDEO_MODES ];

	enum VideoStyle_e
	{
		VS_NONE=0,
		VS_HALF_SCANLINES=1,		// drop 50% scan lines for a more authentic look
		VS_COLOR_VERTICAL_BLEND=2,	// Color "TV Emu" rendering from AppleWin 1.25 (GH#616)
//		VS_TEXT_OPTIMIZED=4,
	};

	enum VideoRefreshRate_e
	{
		VR_NONE,
		VR_50HZ,
		VR_60HZ
	};

	enum VideoFlag_e
	{
		VF_80COL  = 0x00000001,
		VF_DHIRES = 0x00000002,
		VF_HIRES  = 0x00000004,
		VF_80STORE= 0x00000008,
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

#ifdef _MSC_VER
	/// turn of MSVC struct member padding
	#pragma pack(push,1)
	#define PACKED
#else
	#define PACKED // TODO: FIXME: gcc/clang __attribute__
#endif

// TODO: Replace with WinGDI.h / RGBQUAD
struct bgra_t
{
	uint8_t b;
	uint8_t g;
	uint8_t r;
	uint8_t a; // reserved on Win32
};

struct WinBmpHeader_t
{
	// BITMAPFILEHEADER     // Addr Size
	uint8_t  nCookie[2]      ; // 0x00 0x02 BM
	uint32_t nSizeFile       ; // 0x02 0x04 0 = ignore
	uint16_t nReserved1      ; // 0x06 0x02
	uint16_t nReserved2      ; // 0x08 0x02
	uint32_t nOffsetData     ; // 0x0A 0x04
	//                      ==      0x0D (14)

	// BITMAPINFOHEADER
	uint32_t nStructSize     ; // 0x0E 0x04 biSize
	uint32_t nWidthPixels    ; // 0x12 0x04 biWidth
	uint32_t nHeightPixels   ; // 0x16 0x04 biHeight
	uint16_t nPlanes         ; // 0x1A 0x02 biPlanes
	uint16_t nBitsPerPixel   ; // 0x1C 0x02 biBitCount
	uint32_t nCompression    ; // 0x1E 0x04 biCompression 0 = BI_RGB
	uint32_t nSizeImage      ; // 0x22 0x04 0 = ignore
	uint32_t nXPelsPerMeter  ; // 0x26 0x04
	uint32_t nYPelsPerMeter  ; // 0x2A 0x04
	uint32_t nPaletteColors  ; // 0x2E 0x04
	uint32_t nImportantColors; // 0x32 0x04
	//                      ==      0x28 (40)

	// RGBQUAD
	// pixelmap
};

struct WinCIEXYZ
{
	uint32_t r; // fixed point 2.30
	uint32_t g; // fixed point 2.30
	uint32_t b; // fixed point 2.30
};

struct WinBmpHeader4_t
{
	// BITMAPFILEHEADER        // Addr Size
	uint8_t  nCookie[2]      ; // 0x00 0x02 BM
	uint32_t nSizeFile       ; // 0x02 0x04 0 = ignore
	uint16_t nReserved1      ; // 0x06 0x02
	uint16_t nReserved2      ; // 0x08 0x02
	uint32_t nOffsetData     ; // 0x0A 0x04
	//                            ==== 0x0D (14)

	// BITMAPINFOHEADER
	uint32_t nStructSize     ; // 0x0E 0x04 biSize
	uint32_t nWidthPixels    ; // 0x12 0x04 biWidth
	uint32_t nHeightPixels   ; // 0x16 0x04 biHeight
	uint16_t nPlanes         ; // 0x1A 0x02 biPlanes
	uint16_t nBitsPerPixel   ; // 0x1C 0x02 biBitCount
	uint32_t nCompression    ; // 0x1E 0x04 biCompression 0 = BI_RGB
	uint32_t nSizeImage      ; // 0x22 0x04 0 = ignore
	uint32_t nXPelsPerMeter  ; // 0x26 0x04
	uint32_t nYPelsPerMeter  ; // 0x2A 0x04
	uint32_t nPaletteColors  ; // 0x2E 0x04
	uint32_t nImportantColors; // 0x32 0x04
	//                            ==== 0x28 (40)

	//BITMAPV4HEADER new fields
	uint32_t nRedMask        ; // 0x36 0x04
	uint32_t nGreenMask      ; // 0x3A 0x04
	uint32_t nBlueMask       ; // 0x3E 0x04
	uint32_t nAlphaMask      ; // 0x42 0x04
	uint32_t nType           ; // 0x46 0x04

	uint32_t Rx, Ry, Rz      ; // 0x4A 0x0C
	uint32_t Gx, Gy, Gz      ; // 0x56 0x0C
	uint32_t Bx, By, Bz      ; // 0x62 0x0C

	uint32_t nRedGamma       ; // 0x6E 0x04
	uint32_t nGreenGamma     ; // 0x72 0x04
	uint32_t nBlueGamma      ; // 0x76 0x04
};

#ifdef _MSC_VER
	#pragma pack(pop)
#endif

// Globals __________________________________________________________

extern COLORREF   g_nMonochromeRGB;	// saved to Registry
extern uint32_t   g_uVideoMode;
extern DWORD      g_eVideoType;		// saved to Registry
extern uint8_t   *g_pFramebufferbits;

// Prototypes _______________________________________________________

void    VideoBenchmark ();
void    VideoChooseMonochromeColor (); // FIXME: Should be moved to PageConfig and call VideoSetMonochromeColor()
void    VideoDestroy ();
void    VideoDisplayLogo ();
void    VideoInitialize ();
void    VideoRedrawScreenDuringFullSpeed(DWORD dwCyclesThisFrame, bool bInit = false);
void    VideoRedrawScreenAfterFullSpeed(DWORD dwCyclesThisFrame);
void    VideoRedrawScreen (void);
void    VideoRefreshScreen (uint32_t uRedrawWholeScreenVideoMode = 0, bool bRedrawWholeScreen = false);
void    VideoReinitialize (bool bInitVideoScannerAddress = true);
void    VideoResetState ();
enum VideoScanner_e {VS_FullAddr, VS_PartialAddrV, VS_PartialAddrH};
WORD    VideoGetScannerAddress(DWORD nCycles, VideoScanner_e videoScannerAddr = VS_FullAddr);
bool    VideoGetVblBarEx(const DWORD dwCyclesThisFrame);
bool    VideoGetVblBar(const DWORD uExecutedCycles);

bool    VideoGetSW80COL(void);
bool    VideoGetSWDHIRES(void);
bool    VideoGetSWHIRES(void);
bool    VideoGetSW80STORE(void);
bool    VideoGetSWMIXED(void);
bool    VideoGetSWPAGE2(void);
bool    VideoGetSWTEXT(void);
bool    VideoGetSWAltCharSet(void);

void    VideoSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    VideoLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);

extern bool g_bDisplayPrintScreenFileName;
extern bool g_bShowPrintScreenWarningDialog;

void Video_ResetScreenshotCounter( const std::string & pDiskImageFileName );
enum VideoScreenShot_e
{
	SCREENSHOT_560x384 = 0,
	SCREENSHOT_280x192
};
void Video_TakeScreenShot( VideoScreenShot_e ScreenShotType );
void Video_RedrawAndTakeScreenShot( const char* pScreenshotFilename );
void Video_SetBitmapHeader( WinBmpHeader_t *pBmp, int nWidth, int nHeight, int nBitsPerPixel );

BYTE VideoSetMode(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);

const UINT kVideoRomSize2K = 1024*2;
const UINT kVideoRomSize4K = kVideoRomSize2K*2;
bool ReadVideoRomFile(const char* pRomFile);
UINT GetVideoRom(const BYTE*& pVideoRom);
bool GetVideoRomRockerSwitch(void);
void SetVideoRomRockerSwitch(bool state);
bool IsVideoRom4K(void);

void Config_Load_Video(void);
void Config_Save_Video(void);

VideoType_e GetVideoType(void);
void SetVideoType(VideoType_e newVideoType);
VideoStyle_e GetVideoStyle(void);
void SetVideoStyle(VideoStyle_e newVideoStyle);
bool IsVideoStyle(VideoStyle_e mask);

VideoRefreshRate_e GetVideoRefreshRate(void);
void SetVideoRefreshRate(VideoRefreshRate_e rate);

bool DDInit(void);
void DDUninit(void);
