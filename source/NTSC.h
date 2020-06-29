

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
enum	VideoRefreshRate_e;

class NTSC
{
private:

	static unsigned short(*g_pHorzClockOffset)[VIDEO_SCANNER_MAX_HORZ];

	static csbits_t csbits;		// charset, optionally followed by alt charset

	void set_csbits(eApple2Type type);

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
	void updateScreenSingleLores40Simplified(long cycles6502);
	void updateScreenDoubleLores80Simplified(long cycles6502);
	void updateScreenSingleHires40Simplified(long cycles6502);
	void updateScreenDoubleHires80Simplified(long cycles6502);

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

public:
	NTSC();

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
	void     NTSC_VideoInit(uint8_t* pFramebuffer);
	void     NTSC_VideoReinitialize(DWORD cyclesThisFrame, bool bInitVideoScannerAddress);
	static void     NTSC_VideoInitAppleType(eApple2Type type);
	void     NTSC_VideoInitChroma();
	void     NTSC_VideoUpdateCycles(UINT cycles6502);
	void     NTSC_VideoRedrawWholeScreen(void);

	UINT NTSC_GetCyclesPerLine(void);
	UINT NTSC_GetVideoLines(void);
	bool NTSC_IsVisible(void);

	void updateVideoScannerHorzEOLSimple();

	static UINT NTSC_GetCyclesPerFrame(void);

};