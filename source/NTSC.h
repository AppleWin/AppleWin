

// sadly float64 precision is needed
#define real double

#include "VideoTypes.h"

// Understanding the Apple II, Timing Generation and the Video Scanner, Pg 3-11
// Vertical Scanning
// Horizontal Scanning
// "There are exactly 17030 (65 x 262) 6502 cycles in every television scan of an American Apple."
#define VIDEO_SCANNER_MAX_HORZ   65 // TODO: use Video.cpp: kHClocks
#define VIDEO_SCANNER_MAX_VERT  262 // TODO: use Video.cpp: kNTSCScanLines
static const UINT VIDEO_SCANNER_6502_CYCLES = VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_MAX_VERT;

#define VIDEO_SCANNER_MAX_VERT_PAL 312
static const UINT VIDEO_SCANNER_6502_CYCLES_PAL = VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_MAX_VERT_PAL;

class Video;
class RGBMonitor;
enum	VideoRefreshRate_e;



class NTSC
{


public:
	NTSC(Video* _pVideo);
	~NTSC();

	// Globals (Public)
	uint16_t g_nVideoClockVert;
	uint16_t g_nVideoClockHorz;
	uint32_t g_nChromaSize;

	void	NTSC_SetRefreshRate(VideoRefreshRate_e rate);

	void     NTSC_SetVideoMode(uint32_t uVideoModeFlags, bool bDelay = false);
	void     NTSC_SetVideoStyle();
	void     NTSC_SetVideoTextMode(int cols);
	uint32_t* NTSC_VideoGetChromaTable(bool bHueTypeMonochrome, bool bMonitorTypeColorTV);
	void     NTSC_VideoClockResync(const DWORD dwCyclesThisFrame);
	uint16_t NTSC_VideoGetScannerAddress(const ULONG uExecutedCycles);
	uint16_t NTSC_VideoGetScannerAddressForDebugger(void);
	void     NTSC_VideoInit(uint8_t* pFramebuffer, Video* pVideo);
	void     NTSC_VideoReinitialize(DWORD cyclesThisFrame, bool bInitVideoScannerAddress);
	void     NTSC_VideoInitAppleType(eApple2Type type);
	void     NTSC_VideoInitChroma();
	void     NTSC_VideoUpdateCycles(UINT cycles6502);
	void     NTSC_VideoRedrawWholeScreen(void);

	UINT NTSC_GetCyclesPerLine(void);
	UINT NTSC_GetVideoLines(void);
	bool NTSC_IsVisible(void);

	void updateVideoScannerHorzEOLSimple();

	RGBMonitor* GetRGBMonitor();

	static UINT NTSC_GetCyclesPerFrame(void);


private:

	// static variables refer to the global emulation settings ONLY (like PAL or NTSC cycles)
	// everything related to the current video rendering must NOT be static

	// Globals (Private) __________________________________________________
	RGBMonitor* pRGBMonitor = NULL; // child
	Video* pVideo = NULL; //parent

	static int g_nVideoCharSet;
	int g_nVideoMixed = 0;
	int g_nHiresPage = 1;
	int g_nTextPage = 1;

	bool g_bDelayVideoMode = false;	// NB. No need to save to save-state, as it will be done immediately after opcode completes in NTSC_VideoUpdateCycles()
	uint32_t g_uNewVideoModeFlags = 0;


	static UINT g_videoScannerMaxVert;
	static UINT g_videoScanner6502Cycles;

	unsigned short(*g_pHorzClockOffset)[VIDEO_SCANNER_MAX_HORZ];


#define VIDEO_SCANNER_HORZ_COLORBURST_BEG 12
#define VIDEO_SCANNER_HORZ_COLORBURST_END 16

#define VIDEO_SCANNER_HORZ_START 25 // first displayable horz scanner index
#define VIDEO_SCANNER_Y_MIXED   160 // num scanlins for mixed graphics + text
#define VIDEO_SCANNER_Y_DISPLAY 192 // max displayable scanlines

	bgra_t* g_pVideoAddress = 0;
	bgra_t* g_pScanLines[VIDEO_SCANNER_Y_DISPLAY * 2];  // To maintain the 280x192 aspect ratio for 560px width, we double every scan line -> 560x384

	const UINT g_kFrameBufferWidth = GetFrameBufferWidth();


	typedef void (NTSC::*UpdateScreenFunc_t)(long);
	UpdateScreenFunc_t g_apFuncVideoUpdateScanline[VIDEO_SCANNER_Y_DISPLAY];
	UpdateScreenFunc_t g_pFuncUpdateTextScreen = 0; // updateScreenText40;
	UpdateScreenFunc_t g_pFuncUpdateGraphicsScreen = 0; // updateScreenText40;
	UpdateScreenFunc_t g_pFuncModeSwitchDelayed = 0;

#define FuncVideoUpdateScanline(a) (this->*(g_apFuncVideoUpdateScanline[a]))
#define FuncUpdateTextScreen       (this->*g_pFuncUpdateTextScreen)
#define FuncUpdateGraphicsScreen   (this->*g_pFuncUpdateGraphicsScreen)
#define FuncModeSwitchDelayed      (this->*g_pFuncModeSwitchDelayed)

	typedef void (NTSC::*UpdatePixelFunc_t)(uint16_t);
	UpdatePixelFunc_t g_pFuncUpdateBnWPixel = 0; //updatePixelBnWMonitorSingleScanline;
	UpdatePixelFunc_t g_pFuncUpdateHuePixel = 0; //updatePixelHueMonitorSingleScanline;

#define FuncUpdateBnWPixel (this->*g_pFuncUpdateBnWPixel)
#define FuncUpdateHuePixel (this->*g_pFuncUpdateHuePixel)


	uint8_t  g_nTextFlashCounter = 0;
	uint16_t g_nTextFlashMask = 0;

	unsigned g_aPixelMaskGR[16];
	uint16_t g_aPixelDoubleMaskHGR[128]; // hgrbits -> g_aPixelDoubleMaskHGR: 7-bit mono 280 pixels to 560 pixel doubling

	int g_nLastColumnPixelNTSC;
	int g_nColorBurstPixels;

#define INITIAL_COLOR_PHASE 0
	int g_nColorPhaseNTSC = INITIAL_COLOR_PHASE;
	int g_nSignalBitsNTSC = 0;

#define NTSC_NUM_PHASES     4
#define NTSC_NUM_SEQUENCES  4096

	const uint32_t ALPHA32_MASK = 0xFF000000; // Win32: aarrggbb

	bgra_t   g_aBnWMonitor[NTSC_NUM_SEQUENCES];
	bgra_t   g_aHueMonitor[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];
	bgra_t   g_aBnwColorTV[NTSC_NUM_SEQUENCES];
	bgra_t   g_aHueColorTV[NTSC_NUM_PHASES][NTSC_NUM_SEQUENCES];

	// g_aBnWMonitor * g_nMonochromeRGB -> g_aBnWMonitorCustom
	// g_aBnwColorTV * g_nMonochromeRGB -> g_aBnWColorTVCustom
	bgra_t g_aBnWMonitorCustom[NTSC_NUM_SEQUENCES];
	bgra_t g_aBnWColorTVCustom[NTSC_NUM_SEQUENCES];

#define CHROMA_ZEROS 2
#define CHROMA_POLES 2
#define CHROMA_GAIN  7.438011255f // Should this be 7.15909 MHz ?
#define CHROMA_0    -0.7318893645f
#define CHROMA_1     1.2336442711f

	//#define LUMGAIN  1.062635655e+01
	//#define LUMCOEF1  -0.3412038399
	//#define LUMCOEF2  0.9647813115
#define LUMA_ZEROS  2
#define LUMA_POLES  2
#define LUMA_GAIN  13.71331570f   // Should this be 14.318180 MHz ?
#define LUMA_0     -0.3961075449f
#define LUMA_1      1.1044202472f

#define SIGNAL_ZEROS 2
#define SIGNAL_POLES 2
#define SIGNAL_GAIN  7.614490548f  // Should this be 7.15909 MHz ?
#define SIGNAL_0    -0.2718798058f 
#define SIGNAL_1     0.7465656072f 

// Tables
	// Video scanner tables are now runtime-generated using UTAIIe logic
	unsigned short g_aClockVertOffsetsHGR[VIDEO_SCANNER_MAX_VERT_PAL];
	unsigned short g_aClockVertOffsetsTXT[VIDEO_SCANNER_MAX_VERT_PAL / 8];

	unsigned short APPLE_IIP_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ];	// 5 = CEILING(312/64) = CEILING(262/64)
	unsigned short APPLE_IIE_HORZ_CLOCK_OFFSET[5][VIDEO_SCANNER_MAX_HORZ];


	static csbits_t csbits;		// charset, optionally followed by alt charset

	static void set_csbits(eApple2Type type);

	void GenerateVideoTables(void);
	void GenerateBaseColors(baseColors_t pBaseNtscColors);
	void CheckVideoTables(void);
	bool CheckVideoTables2(eApple2Type type, uint32_t mode);
	bool IsNTSC(void);

	static float clampZeroOne(const float& x);

	void      updateFramebufferTVSingleScanline(uint16_t signal, bgra_t* pTable);
	void      updateFramebufferTVDoubleScanline(uint16_t signal, bgra_t* pTable);
	void      updateFramebufferMonitorSingleScanline(uint16_t signal, bgra_t* pTable);
	void      updateFramebufferMonitorDoubleScanline(uint16_t signal, bgra_t* pTable);
	void      updatePixels(uint16_t bits);
	void      updateVideoScannerHorzEOL();
	void      updateVideoScannerAddress();
	uint16_t  getVideoScannerAddressTXT();
	uint16_t  getVideoScannerAddressHGR();

	bool NTSC::GetColorBurst(void);

	void initChromaPhaseTables(void);
	real initFilterChroma(real z);
	real initFilterLuma0(real z);
	real initFilterLuma1(real z);
	real initFilterSignal(real z);
	void initPixelDoubleMasks(void);
	void updateMonochromeTables(uint16_t r, uint16_t g, uint16_t b);

	void updatePixelBnWColorTVSingleScanline(uint16_t compositeSignal);
	void updatePixelBnWColorTVDoubleScanline(uint16_t compositeSignal);
	void updatePixelBnWMonitorSingleScanline(uint16_t compositeSignal);
	void updatePixelBnWMonitorDoubleScanline(uint16_t compositeSignal);
	void updatePixelHueColorTVSingleScanline(uint16_t compositeSignal);
	void updatePixelHueColorTVDoubleScanline(uint16_t compositeSignal);
	void updatePixelHueMonitorSingleScanline(uint16_t compositeSignal);
	void updatePixelHueMonitorDoubleScanline(uint16_t compositeSignal);

	void updateScreenDoubleHires40(long cycles6502);
	void updateScreenDoubleHires80(long cycles6502);
	void updateScreenDoubleLores40(long cycles6502);
	void updateScreenDoubleLores80(long cycles6502);
	void updateScreenSingleHires40(long cycles6502);
	void updateScreenSingleLores40(long cycles6502);
	void updateScreenText40(long cycles6502);
	void updateScreenText80(long cycles6502);
	void updateScreenText40RGB(long cycles6502);
	void updateScreenText80RGB(long cycles6502);
	void updateScreenSingleLores40Simplified(long cycles6502);
	void updateScreenDoubleLores80Simplified(long cycles6502);
	void updateScreenSingleHires40Simplified(long cycles6502);
	void updateScreenDoubleHires80Simplified(long cycles6502);
	void updateScreenSingleHires40Duochrome(long cycles6502);

	uint8_t getCharSetBits(int iChar);
	uint16_t getLoResBits(uint8_t iByte);
	uint32_t getScanlineColor(const uint16_t signal, const bgra_t* pTable);
	uint32_t* getScanlineNextInbetween();
	uint32_t* getScanlinePreviousInbetween();
	uint32_t* getScanlinePrevious();
	uint32_t* getScanlineCurrent();
	void updateColorPhase();
	void updateFlashRate();

	void update7MonoPixels(uint16_t bits);
	void VideoUpdateCycles(int cyclesLeftToUpdate);

};