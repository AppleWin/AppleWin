#include "VideoTypes.h"


const int HIRES_COLUMN_SUBUNIT_SIZE = 16;
const int HIRES_COLUMN_UNIT_SIZE = (HIRES_COLUMN_SUBUNIT_SIZE) * 2;
const int HIRES_NUMBER_COLUMNS = (1 << 5);	// 5 bits


const int SRCOFFS_LORES = 0;							//    0
const int SRCOFFS_HIRES = (SRCOFFS_LORES + 16);		//   16
const int SRCOFFS_DHIRES = (SRCOFFS_HIRES + (HIRES_NUMBER_COLUMNS * HIRES_COLUMN_UNIT_SIZE)); // 1040
const int SRCOFFS_TOTAL = (SRCOFFS_DHIRES + 2560);	// 3600

const int MAX_SOURCE_Y = 256;

#define  SETSOURCEPIXEL(x,y,c)  g_aSourceStartofLine[(y)][(x)] = (c)

// TC: Tried to remove HiresToPalIndex[] translation table, so get purple bars when hires data is: 0x80 0x80...
// . V_CreateLookup_HiResHalfPixel_Authentic() uses both ColorMapping (CM_xxx) indices and Color_Palette_Index_e (HGR_xxx)!
#define DO_OPT_PALETTE 0

enum Color_Palette_Index_e
{
	// hires (don't change order) - For tv emulation HGR Video Mode
#if DO_OPT_PALETTE
	HGR_VIOLET       // HCOLOR=2 VIOLET , 2800: 01 00 55 2A
	, HGR_BLUE         // HCOLOR=6 BLUE   , 3000: 81 00 D5 AA
	, HGR_GREEN        // HCOLOR=1 GREEN  , 2400: 02 00 2A 55
	, HGR_ORANGE       // HCOLOR=5 ORANGE , 2C00: 82 00 AA D5
	, HGR_BLACK
	, HGR_WHITE
#else
	HGR_BLACK
	, HGR_WHITE
	, HGR_BLUE         // HCOLOR=6 BLUE   , 3000: 81 00 D5 AA
	, HGR_ORANGE       // HCOLOR=5 ORANGE , 2C00: 82 00 AA D5
	, HGR_GREEN        // HCOLOR=1 GREEN  , 2400: 02 00 2A 55
	, HGR_VIOLET       // HCOLOR=2 VIOLET , 2800: 01 00 55 2A
#endif

// TV emu
, HGR_GREY1
, HGR_GREY2
, HGR_YELLOW
, HGR_AQUA
, HGR_PURPLE
, HGR_PINK
// lores & dhires
, BLACK
, DEEP_RED
, DARK_BLUE
, MAGENTA
, DARK_GREEN
, DARK_GRAY
, BLUE
, LIGHT_BLUE
, BROWN
, ORANGE
, LIGHT_GRAY
, PINK
, GREEN
, YELLOW
, AQUA
, WHITE
};

// __ Map HGR color index to Palette index
enum ColorMapping
{
	CM_Violet
	, CM_Blue
	, CM_Green
	, CM_Orange
	, CM_Black
	, CM_White
	, NUM_COLOR_MAPPING
};

const BYTE HiresToPalIndex[NUM_COLOR_MAPPING] =
{
	  HGR_VIOLET
	, HGR_BLUE
	, HGR_GREEN
	, HGR_ORANGE
	, HGR_BLACK
	, HGR_WHITE
};

const BYTE LoresResColors[16] = {
		BLACK,     DEEP_RED, DARK_BLUE, MAGENTA,
		DARK_GREEN,DARK_GRAY,BLUE,      LIGHT_BLUE,
		BROWN,     ORANGE,   LIGHT_GRAY,PINK,
		GREEN,     YELLOW,   AQUA,      WHITE
};

const BYTE DoubleHiresPalIndex[16] = {
		BLACK,   DARK_BLUE, DARK_GREEN,BLUE,
		BROWN,   LIGHT_GRAY,GREEN,     AQUA,
		DEEP_RED,MAGENTA,   DARK_GRAY, LIGHT_BLUE,
		ORANGE,  PINK,      YELLOW,    WHITE
};

class RGBMonitor
{
public:
	RGBMonitor();
	~RGBMonitor();

	void UpdateHiResCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, Video* pVideo);
	void UpdateDHiResCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, bool updateAux, bool updateMain);
	int UpdateDHiRes160Cell(int x, int y, uint16_t addr, bgra_t* pVideoAddress);
	void UpdateLoResCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress);
	void UpdateDLoResCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress);


	void VideoInitializeOriginal(baseColors_t pBaseNtscColors);

	void RGB_SetVideoMode(WORD address);
	bool RGB_Is140Mode(void);
	bool RGB_Is160Mode(void);
	bool RGB_IsMixMode(void);
	bool RGB_Is560Mode(void);
	bool RGB_IsMixModeInvertBit7(void);
	void RGB_ResetState(void);
	static void RGB_SetInvertBit7(bool state);

	void RGB_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	void RGB_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT cardVersion);

private:
	UINT g_rgbFlags = 0;
	UINT g_rgbMode = 0;
	WORD g_rgbPrevAN3Addr = 0;
	bool g_rgbSet80COL = false;
	static bool g_rgbInvertBit7;

	LPBYTE	g_aSourceStartofLine[MAX_SOURCE_Y];
	void	V_CreateLookup_DoubleHires();
	void	V_CreateLookup_Lores();
	void V_CreateLookup_HiResHalfPixel_Authentic(VideoType_e videoType);

	// For AppleWin 1.25 "tv emulation" HGR Video Mode

	static const UINT HGR_MATRIX_YOFFSET = 2;

	BYTE hgrpixelmatrix[FRAMEBUFFER_W][FRAMEBUFFER_H / 2 + 2 * HGR_MATRIX_YOFFSET];	// 2 extra scan lines on top & bottom
	BYTE colormixbuffer[6];		// 6 hires colours
	WORD colormixmap[6][6][6];	// top x middle x bottom

	LPBYTE g_pSourcePixels = NULL;

	BYTE MixColors(BYTE c1, BYTE c2);
	void CreateColorMixMap(void);
	void MixColorsVertical(int matx, int maty, bool isSWMIXED);
	void CopyMixedSource(int x, int y, int sx, int sy, bgra_t* pVideoAddress);
	void CopySource(int w, int h, int sx, int sy, bgra_t* pVideoAddress, const int nSrcAdjustment);
	void V_CreateDIBSections(void);
};