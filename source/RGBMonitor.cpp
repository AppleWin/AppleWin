// Sync'd with 1.25.0.4 source

#include "StdAfx.h"

#include "RGBMonitor.h"
#include "Memory.h" // MemGetMainPtr() MemGetAuxPtr()
#include "Interface.h"
#include "Card.h"
#include "YamlHelper.h"


// RGB videocards types

static RGB_Videocard_e g_RGBVideocard = RGB_Videocard_e::Apple;
static int g_nTextFBMode = 0; // F/B Text
static int g_nRegularTextFG = 15; // Default TEXT color
static int g_nRegularTextBG = 0; // Default TEXT background color

const int HIRES_COLUMN_SUBUNIT_SIZE = 16;
const int HIRES_COLUMN_UNIT_SIZE = (HIRES_COLUMN_SUBUNIT_SIZE)*2;
const int HIRES_NUMBER_COLUMNS = (1<<5);	// 5 bits


const int SRCOFFS_LORES   = 0;							//    0
const int SRCOFFS_HIRES   = (SRCOFFS_LORES  + 16);		//   16
const int SRCOFFS_DHIRES  = (SRCOFFS_HIRES  + (HIRES_NUMBER_COLUMNS*HIRES_COLUMN_UNIT_SIZE)); // 1040
const int SRCOFFS_TOTAL   = (SRCOFFS_DHIRES + 2560);	// 3600

const int MAX_SOURCE_Y = 256;
static LPBYTE        g_aSourceStartofLine[ MAX_SOURCE_Y ];
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

const BYTE HiresToPalIndex[ NUM_COLOR_MAPPING ] =
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

static RGBQUAD* g_pPaletteRGB;

static RGBQUAD PaletteRGB_NTSC[] =
{
// hires
#if DO_OPT_PALETTE
	SETRGBCOLOR(/*MAGENTA,   */ 0xC7,0x34,0xFF), // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF
	SETRGBCOLOR(/*BLUE,      */ 0x0D,0xA1,0xFF), // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
	SETRGBCOLOR(/*GREEN,     */ 0x38,0xCB,0x00), // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
	SETRGBCOLOR(/*ORANGE,    */ 0xF2,0x5E,0x00), // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00), // For TV emulation HGR Video Mode
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
#else
// Note: this is a placeholder. This palette is overwritten by VideoInitializeOriginal()
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00), // For TV emulation HGR Video Mode
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
	SETRGBCOLOR(/*BLUE,      */ 0x0D,0xA1,0xFF), // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
	SETRGBCOLOR(/*ORANGE,    */ 0xF2,0x5E,0x00), // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
	SETRGBCOLOR(/*GREEN,     */ 0x38,0xCB,0x00), // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
	SETRGBCOLOR(/*MAGENTA,   */ 0xC7,0x34,0xFF), // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF
#endif

// TV emu
	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00), // 0xD0,0xB0,0x10 -> 0x9E,0x9E,0x00
	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A), // 0x20,0xB0,0xB0 -> 0x00,0xCD,0x4A
	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF), // 0x60,0x50,0xE0 -> 0x61,0x61,0xFF
	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5), // 0xD0,0x40,0xA0 -> 0xFF,0x32,0xB5

// lores & dhires
// Note: this is a placeholder. This palette is overwritten by VideoInitializeOriginal()
	SETRGBCOLOR(/*BLACK,*/      0x00,0x00,0x00), // 0
	SETRGBCOLOR(/*DEEP_RED,*/   0x9D,0x09,0x66), // 0xD0,0x00,0x30 -> Linards Tweaked 0x9D,0x09,0x66
	SETRGBCOLOR(/*DARK_BLUE,*/  0x2A,0x2A,0xE5), // 4 // Linards Tweaked 0x00,0x00,0x80 -> 0x2A,0x2A,0xE5
	SETRGBCOLOR(/*MAGENTA,*/    0xC7,0x34,0xFF), // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF
	SETRGBCOLOR(/*DARK_GREEN,*/ 0x00,0x80,0x00), // 2 // not used
	SETRGBCOLOR(/*DARK_GRAY,*/  0x80,0x80,0x80), // F8
	SETRGBCOLOR(/*BLUE,*/       0x0D,0xA1,0xFF), // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
	SETRGBCOLOR(/*LIGHT_BLUE,*/ 0xAA,0xAA,0xFF), // 0x60,0xA0,0xFF -> Linards Tweaked 0xAA,0xAA,0xFF
	SETRGBCOLOR(/*BROWN,*/      0x55,0x55,0x00), // 0x80,0x50,0x00 -> Linards Tweaked 0x55,0x55,0x00
	SETRGBCOLOR(/*ORANGE,*/     0xF2,0x5E,0x00), // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
	SETRGBCOLOR(/*LIGHT_GRAY,*/ 0xC0,0xC0,0xC0), // 7 // GR: COLOR=10
	SETRGBCOLOR(/*PINK,*/       0xFF,0x89,0xE5), // 0xFF,0x90,0x80 -> Linards Tweaked 0xFF,0x89,0xE5
	SETRGBCOLOR(/*GREEN,*/      0x38,0xCB,0x00), // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
	SETRGBCOLOR(/*YELLOW,*/     0xD5,0xD5,0x1A), // FB Linards Tweaked 0xFF,0xFF,0x00 -> 0xD5,0xD5,0x1A
	SETRGBCOLOR(/*AQUA,*/       0x62,0xF6,0x99), // 0x40,0xFF,0x90 -> Linards Tweaked 0x62,0xF6,0x99
	SETRGBCOLOR(/*WHITE,*/      0xFF,0xFF,0xFF),
};

// Le Chat Mauve Feline's palette
// extracted from a white-balanced RGB video capture
static RGBQUAD PaletteRGB_Feline[] =
{
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00),
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),
	SETRGBCOLOR(/*BLUE,      */ 0x00,0x8A,0xB5),
	SETRGBCOLOR(/*ORANGE,    */ 0xFF,0x72,0x47),
	SETRGBCOLOR(/*GREEN,     */ 0x6F,0xE6,0x2C),
	SETRGBCOLOR(/*MAGENTA,   */ 0xAA,0x1A,0xD1),

	// TV emu
	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00),
	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A),
	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF),
	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5),

	// Feline
	SETRGBCOLOR(/*BLACK,*/      0x00,0x00,0x00),
	SETRGBCOLOR(/*DEEP_RED,*/   0xAC,0x12,0x4C),
	SETRGBCOLOR(/*DARK_BLUE,*/  0x00,0x07,0x83),
	SETRGBCOLOR(/*MAGENTA,*/    0xAA,0x1A,0xD1),
	SETRGBCOLOR(/*DARK_GREEN,*/ 0x00,0x83,0x2F),
	SETRGBCOLOR(/*DARK_GRAY,*/  0x9F,0x97,0x7E),
	SETRGBCOLOR(/*BLUE,*/       0x00,0x8A,0xB5),
	SETRGBCOLOR(/*LIGHT_BLUE,*/ 0x9F,0x9E,0xFF),
	SETRGBCOLOR(/*BROWN,*/      0x7A,0x5F,0x00),
	SETRGBCOLOR(/*ORANGE,*/     0xFF,0x72,0x47),
	SETRGBCOLOR(/*LIGHT_GRAY,*/ 0x78,0x68,0x7F),
	SETRGBCOLOR(/*PINK,*/       0xFF,0x7A,0xCF),
	SETRGBCOLOR(/*GREEN,*/      0x6F,0xE6,0x2C),
	SETRGBCOLOR(/*YELLOW,*/     0xFF,0xF6,0x7B),
	SETRGBCOLOR(/*AQUA,*/       0x6C,0xEE,0xB2),
	SETRGBCOLOR(/*WHITE,*/      0xFF,0xFF,0xFF),
};

//===========================================================================

static void V_CreateLookup_DoubleHires ()
{
#define OFFSET  3
#define SIZE    10

  for (int column = 0; column < 256; column++) {
    int coloffs = SIZE * column;
    for (unsigned byteval = 0; byteval < 256; byteval++) {
      int color[SIZE];
      memset(color, 0, sizeof(color));
      unsigned pattern = MAKEWORD(byteval,column);
      int pixel;
      for (pixel = 1; pixel < 15; pixel++) {
        if (pattern & (1 << pixel)) {
          int pixelcolor = 1 << ((pixel-OFFSET) & 3);
          if ((pixel >=  OFFSET+2) && (pixel < SIZE+OFFSET+2) && (pattern & (0x7 << (pixel-4))))
            color[pixel-(OFFSET+2)] |= pixelcolor;
          if ((pixel >=  OFFSET+1) && (pixel < SIZE+OFFSET+1) && (pattern & (0xF << (pixel-4))))
            color[pixel-(OFFSET+1)] |= pixelcolor;
          if ((pixel >=  OFFSET+0) && (pixel < SIZE+OFFSET+0))
            color[pixel-(OFFSET+0)] |= pixelcolor;
          if ((pixel >=  OFFSET-1) && (pixel < SIZE+OFFSET-1) && (pattern & (0xF << (pixel+1))))
            color[pixel-(OFFSET-1)] |= pixelcolor;
          if ((pixel >=  OFFSET-2) && (pixel < SIZE+OFFSET-2) && (pattern & (0x7 << (pixel+2))))
            color[pixel-(OFFSET-2)] |= pixelcolor;
        }
      }

#if 0
	  if (g_eVideoType == VT_COLOR_TEXT_OPTIMIZED)
	  {
	    // Activate for fringe reduction on white HGR text - drawback: loss of color mix patterns in HGR Video Mode.
		for (pixel = 0; pixel < 13; pixel++)
		{
		  if ((pattern & (0xF << pixel)) == (unsigned)(0xF << pixel))
			for (int pos = pixel; pos < pixel + 4; pos++)
			  if (pos >= OFFSET && pos < SIZE+OFFSET)
				color[pos-OFFSET] = 15;
		}
	  }
#endif

      int y = byteval;
      for (int x = 0; x < SIZE; x++) {
        SETSOURCEPIXEL(SRCOFFS_DHIRES+coloffs+x,y  ,DoubleHiresPalIndex[ color[x] ]);
      }
    }
  }
#undef SIZE
#undef OFFSET
}

//===========================================================================

void V_CreateLookup_Lores()
{
	for (int color = 0; color < 16; color++)
		for (int x = 0; x < 16; x++)
			for (int y = 0; y < 16; y++)
				SETSOURCEPIXEL(SRCOFFS_LORES+x,(color << 4)+y,LoresResColors[color]);
}

//===========================================================================

// Lookup Table:
// y (0-255) * 32 columns of 32 bytes
// . each column is: high-bit (prev byte) & 2 pixels from previous byte & 2 pixels from next byte
// . each 32-byte unit is 2 * 16-byte sub-units: 16 bytes for even video byte & 16 bytes for odd video byte
//   . where 16 bytes represent the 7 Apple pixels, expanded to 14 pixels
//		currHighBit=0: {14 pixels + 2 pad} * 2
//		currHighBit=1: {1 pixel + 14 pixels + 1 pad} * 2
//   . and each byte is an index into the colour palette

void V_CreateLookup_HiResHalfPixel_Authentic(VideoType_e videoType)
{
	// high-bit & 2-bits from previous byte, 2-bits from next byte = 2^5 = 32 total permutations
	for (int iColumn = 0; iColumn < HIRES_NUMBER_COLUMNS; iColumn++)
	{
		const int offsetx = iColumn * HIRES_COLUMN_UNIT_SIZE; // every column is 32 bytes wide
		const int prevHighBit = (iColumn >= 16) ? 1 : 0;
		int aPixels[11]; // c2 c3 b6 b5 b4 b3 b2 b1 b0 c0 c1

		aPixels[ 0] = iColumn & 4; // previous byte, 2nd last pixel
		aPixels[ 1] = iColumn & 8; // previous byte, last pixel
		aPixels[ 9] = iColumn & 1; // next byte, first pixel
		aPixels[10] = iColumn & 2; // next byte, second pixel

		for (unsigned int iByte = 0; iByte < 256; iByte++)
		{
			// Convert raw pixel iByte value to binary and stuff into bit array of pixels on off
			for (int iPixel = 2, nBitMask = 1; iPixel < 9; iPixel++)
			{
				aPixels[iPixel] = ((iByte & nBitMask) != 0);
				nBitMask <<= 1;
			}

			const int currHighBit = (iByte >> 7) & 1;
			const int y = iByte;

			// Fixup missing pixels that normally have been scan-line shifted -- Apple "half-pixel" -- but crosses video byte boundaries.
			// NB. Setup first byte in each 16-byte sub-unit
			if( currHighBit )
			{
				if ( aPixels[1] ) // prev pixel on?
				{
					if (aPixels[2] || aPixels[0]) // White if pixel from previous byte and first pixel of this byte is on
					{
						SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+0 ,y  , HGR_WHITE );
						SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+HIRES_COLUMN_SUBUNIT_SIZE,y  , HGR_WHITE );
					}
					else
					{
						if ( !prevHighBit )	// GH#616
						{
							// colour the half-pixel black (was orange - not good for Nox Archaist, eg. 2000:00 40 E0; 2000:00 40 9E)
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+0 ,y  , HGR_BLACK );
						}
						else
						{
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+0 ,y  , HGR_ORANGE ); // left half of orange pixels
						}
						SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+HIRES_COLUMN_SUBUNIT_SIZE,y  , HGR_BLUE ); // right half of blue pixels 4, 11, 18, ...
					}
				}
				else if ( aPixels[0] ) // prev prev pixel on
				{
					if ( aPixels[2] )
					{
						if ((videoType == VT_COLOR_IDEALIZED) || ( !aPixels[3] ))
						{ 
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+0 ,y  , HGR_BLUE ); // 2000:D5 AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+HIRES_COLUMN_SUBUNIT_SIZE,y  , HGR_ORANGE ); // 2000: AA D5
						}
					}
				}
			}

			//

			int x = currHighBit;

			for (int odd = 0; odd < 2; odd++)	// even then odd sub-units
			{
				if (odd)
					x = HIRES_COLUMN_SUBUNIT_SIZE + currHighBit;

				for (int iPixel = 2; iPixel < 9; iPixel++)
				{
					int color = CM_Black;
					if (aPixels[iPixel]) // pixel on
					{
						color = CM_White; 
						if (aPixels[iPixel-1] || aPixels[iPixel+1]) // adjacent pixels are always white
							color = CM_White; 
						else
							color = ((odd ^ (iPixel&1)) << 1) | currHighBit; // map raw color to our hi-res colors
					}
					else if (aPixels[iPixel-1] && aPixels[iPixel+1]) // IF prev_pixel && next_pixel THEN
					{
						// Activate fringe reduction on white HGR text - drawback: loss of color mix patterns in HGR video mode.
						if (
							(videoType == VT_COLOR_IDEALIZED) // Fill in colors in between white pixels
						|| !(aPixels[iPixel-2] && aPixels[iPixel+2]) ) // VT_COLOR_TEXT_OPTIMIZED -> Don't fill in colors in between white
						{
							color = ((odd ^ !(iPixel&1)) << 1) | currHighBit;	// No white HGR text optimization
						}
					}

					// Each HGR 7M pixel is a left 14M & right 14M DHGR pixel
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x  ,y  ,HiresToPalIndex[color]); // Color for left 14M pixel
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+1,y  ,HiresToPalIndex[color]); // Color for right 14M pixel

					x += 2;
				}
			} // even/odd sub-units
		} // iByte
	} // iColumn
}

//===========================================================================

// For AppleWin 1.25 "tv emulation" HGR Video Mode

const UINT FRAMEBUFFER_W = 560;
const UINT FRAMEBUFFER_H = 384;
const UINT HGR_MATRIX_YOFFSET = 2;

static BYTE hgrpixelmatrix[FRAMEBUFFER_W][FRAMEBUFFER_H/2 + 2 * HGR_MATRIX_YOFFSET];	// 2 extra scan lines on top & bottom
static BYTE colormixbuffer[6];		// 6 hires colours
static WORD colormixmap[6][6][6];	// top x middle x bottom

BYTE MixColors(BYTE c1, BYTE c2)
{
#define COMBINATION(c1,c2,ref1,ref2) (((c1)==(ref1)&&(c2)==(ref2)) || ((c1)==(ref2)&&(c2)==(ref1)))

	if (c1 == c2)
		return c1;
	if (COMBINATION(c1,c2,HGR_BLUE,HGR_ORANGE))
		return HGR_GREY1;
	else if (COMBINATION(c1,c2,HGR_GREEN,HGR_VIOLET))
		return HGR_GREY2;
	else if (COMBINATION(c1,c2,HGR_ORANGE,HGR_GREEN))
		return HGR_YELLOW;
	else if (COMBINATION(c1,c2,HGR_BLUE,HGR_GREEN))
		return HGR_AQUA;
	else if (COMBINATION(c1,c2,HGR_BLUE,HGR_VIOLET))
		return HGR_PURPLE;
	else if (COMBINATION(c1,c2,HGR_ORANGE,HGR_VIOLET))
		return HGR_PINK;
	else
		return WHITE;	// visible failure indicator

#undef COMBINATION
}

static void CreateColorMixMap(void)
{
	const int FROM_NEIGHBOUR = 0x00;
	const int MIX_THRESHOLD = HGR_BLUE; // (skip) bottom 2 HGR colors

	for (int t=0; t<6; t++)	// Color_Palette_Index_e::HGR_BLACK(0) ... Color_Palette_Index_e::HGR_VIOLET(5)
	{
		for (int m=0; m<6; m++)
		{
			for (int b=0; b<6; b++)
			{
				BYTE cTop = t;
				BYTE cMid = m;
				BYTE cBot = b;

				WORD mixTop, mixBot;

				if (cMid < MIX_THRESHOLD)
				{
					mixTop = mixBot = cMid;
				}
				else
				{
					if (cTop < MIX_THRESHOLD)
						mixTop = FROM_NEIGHBOUR;
					else
						mixTop = MixColors(cMid,cTop);

					if (cBot < MIX_THRESHOLD)
						mixBot = FROM_NEIGHBOUR;
					else
						mixBot = MixColors(cMid,cBot);

					if (mixTop == FROM_NEIGHBOUR && mixBot != FROM_NEIGHBOUR)
						mixTop = mixBot;
					else if (mixBot == FROM_NEIGHBOUR && mixTop != FROM_NEIGHBOUR)
						mixBot = mixTop;
					else if (mixBot == FROM_NEIGHBOUR && mixTop == FROM_NEIGHBOUR)
						mixBot = mixTop = cMid;
				}

				colormixmap[t][m][b] = (mixTop << 8) | mixBot;
			}
		}
	}
}

static void MixColorsVertical(int matx, int maty, bool isSWMIXED)
{
	int bot1idx, bot2idx;

	if (isSWMIXED && maty > 159)
	{
		if (maty < 161)
		{
			bot1idx = hgrpixelmatrix[matx][maty+1] & 0x0F;
			bot2idx = 0;
		}
		else
		{
			bot1idx = bot2idx = 0;
		}
	}
	else
	{
		bot1idx = hgrpixelmatrix[matx][maty+1] & 0x0F;
		bot2idx = hgrpixelmatrix[matx][maty+2] & 0x0F;
	}

	WORD twoHalfPixel = colormixmap[hgrpixelmatrix[matx][maty-2] & 0x0F]
								   [hgrpixelmatrix[matx][maty-1] & 0x0F]
								   [hgrpixelmatrix[matx][maty  ] & 0x0F];
	colormixbuffer[0] = (twoHalfPixel & 0xFF00) >> 8;
	colormixbuffer[1] = (twoHalfPixel & 0x00FF);

	twoHalfPixel = colormixmap[hgrpixelmatrix[matx][maty-1] & 0x0F]
							  [hgrpixelmatrix[matx][maty  ] & 0x0F]
							  [bot1idx];
	colormixbuffer[2] = (twoHalfPixel & 0xFF00) >> 8;
	colormixbuffer[3] = (twoHalfPixel & 0x00FF);

	twoHalfPixel = colormixmap[hgrpixelmatrix[matx][maty  ] & 0x0F]
							  [bot1idx]
							  [bot2idx];
	colormixbuffer[4] = (twoHalfPixel & 0xFF00) >> 8;
	colormixbuffer[5] = (twoHalfPixel & 0x00FF);
}

static void CopyMixedSource(int x, int y, int sx, int sy, bgra_t *pVideoAddress)
{
	const BYTE* const pSrc = g_aSourceStartofLine[ sy ] + sx;

	const int matx = x*14;
	const int maty = HGR_MATRIX_YOFFSET + y;
	const bool isSWMIXED = GetVideo().VideoGetSWMIXED();

	// transfer 14 pixels (i.e. the visible part of an apple hgr-byte) from row to pixelmatrix
	for (int nBytes=13; nBytes>=0; nBytes--)
	{
		hgrpixelmatrix[matx+nBytes][maty] = *(pSrc+nBytes);
	}

	const bool bIsHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);
	const UINT frameBufferWidth = GetVideo().GetFrameBufferWidth();

	for (int nBytes=13; nBytes>=0; nBytes--)
	{
		// color mixing between adjacent scanlines at current x position
		MixColorsVertical(matx+nBytes, maty, isSWMIXED);	//Post: colormixbuffer[]

		UINT32* pDst = (UINT32*) pVideoAddress;

		for (int h=HGR_MATRIX_YOFFSET; h<=HGR_MATRIX_YOFFSET+1; h++)
		{
			if (bIsHalfScanLines && (h & 1))
			{
				// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
				*(pDst+nBytes) = OPAQUE_BLACK;
			}
			else
			{
				_ASSERT( colormixbuffer[h] < (sizeof(PaletteRGB_NTSC)/sizeof(PaletteRGB_NTSC[0])) );
				const RGBQUAD& rRGB = g_pPaletteRGB[ colormixbuffer[h] ];
				*(pDst+nBytes) = *reinterpret_cast<const UINT32 *>(&rRGB);
			}

			pDst -= frameBufferWidth;
		}
	}
}

//===========================================================================

// Pre: nSrcAdjustment: for 160-color images, src is +1 compared to dst
static void CopySource(int w, int h, int sx, int sy, bgra_t *pVideoAddress, const int nSrcAdjustment = 0)
{
	UINT32* pDst = (UINT32*) pVideoAddress;
	const BYTE* const pSrc = g_aSourceStartofLine[ sy ] + sx;

	const bool bIsHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);
	const UINT frameBufferWidth = GetVideo().GetFrameBufferWidth();

	while (h--)
	{
		if (bIsHalfScanLines && !(h & 1))
		{
			// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
			std::fill(pDst, pDst + w, OPAQUE_BLACK);
		}
		else
		{
			for (int nBytes=0; nBytes<w; ++nBytes)
			{
				_ASSERT( *(pSrc+nBytes+nSrcAdjustment) < (sizeof(PaletteRGB_NTSC)/sizeof(PaletteRGB_NTSC[0])) );
				const RGBQUAD& rRGB = g_pPaletteRGB[ *(pSrc+nBytes+nSrcAdjustment) ];
				*(pDst+nBytes) = *reinterpret_cast<const UINT32 *>(&rRGB);
			}
		}

		pDst -= frameBufferWidth;
	}
}

//===========================================================================

#define HIRES_COLUMN_OFFSET (((byteval1 & 0xE0) << 2) | ((byteval3 & 0x03) << 5))	// (prevHighBit | last 2 pixels | next 2 pixels) * HIRES_COLUMN_UNIT_SIZE

void UpdateHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	uint8_t *pMain = MemGetMainPtr(addr);
	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 =            *(pMain);
	BYTE byteval3 = (x < 39) ? *(pMain+1) : 0;

	if (GetVideo().GetVideoMode() & VF_DHIRES)	// ie. VF_DHIRES=1, VF_HIRES=1, VF_80COL=0 - NTSC.cpp refers to this as "DoubleHires40"
	{
		byteval1 &= 0x7f;
		byteval2 &= 0x7f;
		byteval3 &= 0x7f;
	}

	if (GetVideo().IsVideoStyle(VS_COLOR_VERTICAL_BLEND))
	{
		CopyMixedSource(x, y, SRCOFFS_HIRES+HIRES_COLUMN_OFFSET+((x & 1)*HIRES_COLUMN_SUBUNIT_SIZE), (int)byteval2, pVideoAddress);
	}
	else
	{
		CopySource(14,2, SRCOFFS_HIRES+HIRES_COLUMN_OFFSET+((x & 1)*HIRES_COLUMN_SUBUNIT_SIZE), (int)byteval2, pVideoAddress);
	}
}

//===========================================================================

#define COLOR  ((xpixel + PIXEL) & 3)
#define VALUE  (dwordval >> (4 + PIXEL - COLOR))

void UpdateDHiResCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, bool updateAux, bool updateMain)
{
	const int xpixel = x * 14;

	uint8_t* pAux = MemGetAuxPtr(addr);
	uint8_t* pMain = MemGetMainPtr(addr);

	BYTE byteval1 = (x > 0) ? *(pMain - 1) : 0;
	BYTE byteval2 = *pAux;
	BYTE byteval3 = *pMain;
	BYTE byteval4 = (x < 39) ? *(pAux + 1) : 0;

	DWORD dwordval = (byteval1 & 0x70) | ((byteval2 & 0x7F) << 7) |
		((byteval3 & 0x7F) << 14) | ((byteval4 & 0x07) << 21);

#define PIXEL  0
	if (updateAux)
	{
		CopySource(7, 2, SRCOFFS_DHIRES + 10 * HIBYTE(VALUE) + COLOR, LOBYTE(VALUE), pVideoAddress);
		pVideoAddress += 7;
	}
#undef PIXEL

#define PIXEL  7
	if (updateMain)
	{
		CopySource(7, 2, SRCOFFS_DHIRES + 10 * HIBYTE(VALUE) + COLOR, LOBYTE(VALUE), pVideoAddress);
	}
#undef PIXEL
}

//===========================================================================
// RGB videocards HGR

void UpdateHiResRGBCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress)
{
	const int xpixel = x * 14;
	int xoffset = x & 1; // offset to start of the 2 bytes
	addr -= xoffset;

	uint8_t* pMain = MemGetMainPtr(addr);

	// We need all 28 bits because each pixel needs a three bit evaluation
	uint8_t byteval1 = (x < 2 ? 0 : *(pMain - 1));
	uint8_t byteval2 = *pMain;
	uint8_t byteval3 = *(pMain + 1);
	uint8_t byteval4 = (x >= 38 ? 0 : *(pMain + 2));
	
	// all 28 bits chained
	DWORD dwordval = (byteval1 & 0x7F) | ((byteval2 & 0x7F) << 7) | ((byteval3 & 0x7F) << 14) | ((byteval4 & 0x7F) << 21);

	// Extraction of 14 color pixels
	UINT32 colors[14];
	int color = 0;
	DWORD dwordval_tmp = dwordval;
	dwordval_tmp = dwordval_tmp >> 7;
	bool offset = (byteval2 & 0x80) ? true : false;
	for (int i = 0; i < 14; i++)
	{
		if (i == 7) offset = (byteval3 & 0x80) ? true : false;
		color = dwordval_tmp & 0x3;
		// Two cases because AppleWin's palette is in a strange order
		if (offset)
			colors[i] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[1 + color]);
		else
			colors[i] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[6 - color]);
		if (i%2) dwordval_tmp >>= 2;
	}
	// Black and White
	UINT32 bw[2];
	bw[0] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[0]);
	bw[1] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[1]);

	DWORD mask  =  0x01C0; //  00|000001 1|1000000
	DWORD chck1 =  0x0140; //  00|000001 0|1000000
	DWORD chck2 =  0x0080; //  00|000000 1|0000000

	// HIRES render in RGB works on a pixel-basis (1-bit data in framebuffer)
	// The pixel can be 'color', if it makes a 101 or 010 pattern with the two neighbour bits
	// In all other cases, it's black if 0 and white if 1
	// The value of 'color' is defined on a 2-bits basis

	UINT32* pDst = (UINT32*)pVideoAddress;

	if (xoffset)
	{
		// Second byte of the 14 pixels block
		dwordval = dwordval >> 7;
		xoffset = 7;
	}

	for (int i = xoffset; i < xoffset+7; i++)
	{
		if (((dwordval & mask) == chck1) || ((dwordval & mask) == chck2))
		{
			// Color pixel
			*(pDst) = colors[i];
			*(pDst + 1) = *(pDst);
			pDst += 2;
		}
		else
		{
			// B&W pixel
			*(pDst) = bw[(dwordval & chck2 ? 1 : 0)];
			*(pDst + 1) = *(pDst);
			pDst += 2;
		}
		// Next pixel
		dwordval = dwordval >> 1;
	}

	const bool bIsHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);

	// Second line
	UINT32* pSrc = (UINT32*)pVideoAddress;
	pDst = pSrc - GetVideo().GetFrameBufferWidth();
	if (bIsHalfScanLines)
	{
		// Scanlines
		std::fill(pDst, pDst + 14, OPAQUE_BLACK);
	}
	else
	{
		for (int i = 0; i < 14; i++)
			*(pDst + i) = *(pSrc + i);
	}
}

static bool g_dhgrLastCellIsColor = true;
static int g_dhgrLastBit = 0;

void UpdateDHiResCellRGB(int x, int y, uint16_t addr, bgra_t* pVideoAddress, bool isMixMode, bool isBit7Inversed)
{
	const int xpixel = x * 14;
	int xoffset = x & 1; // offset to start of the 2 bytes
	addr -= xoffset;

	uint8_t* pAux = MemGetAuxPtr(addr);
	uint8_t* pMain = MemGetMainPtr(addr);

	// We need all 28 bits because one 4-bits pixel overlaps two 14-bits cells
	uint8_t byteval1 = *pAux;
	uint8_t byteval2 = *pMain;
	uint8_t byteval3 = *(pAux + 1);
	uint8_t byteval4 = *(pMain + 1);

	// all 28 bits chained
	DWORD dwordval = (byteval1 & 0x7F) | ((byteval2 & 0x7F) << 7) | ((byteval3 & 0x7F) << 14) | ((byteval4 & 0x7F) << 21);

	// Extraction of 7 color pixels and 7x4 bits
	int bits[7];
	UINT32 colors[7];
	int color = 0;
	DWORD dwordval_tmp = dwordval;
	for (int i = 0; i < 7; i++)
	{
		bits[i] = dwordval_tmp & 0xF;
		color = ((bits[i] & 7) << 1) | ((bits[i] & 8) >> 3); // DHGR colors are rotated 1 bit to the right
		colors[i] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[12 + color]);
		dwordval_tmp >>= 4;
	}
	UINT32 bw[2];
	bw[0] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[12 + 0]);
	bw[1] = *reinterpret_cast<const UINT32*>(&g_pPaletteRGB[12 + 15]);

	if (isBit7Inversed)
	{
		// Invert mixed mode detection
		byteval1 = ~byteval1;
		byteval2 = ~byteval2;
		byteval3 = ~byteval3;
		byteval4 = ~byteval4;
	}

	// RGB DHGR is quite a mess:
	// Color mode is a real 140x192 RGB mode with no color fringe (ref. patent US4631692, "THE 140x192 VIDEO MODE")
	// BW mode is a real 560x192 monochrome mode
	// Mixed mode seems easy but has a few traps since it's based on 4-bits cells coded into 7-bits bytes:
	//   - Bit 7 of each byte defines the mode of the following 7 bits (BW or Color);
	//   - BW pixels are 1 bit wide, color pixels are usually 4 bits wide;
	//   - A color pixel can be less than 4 bits wide if it crosses a byte boundary and falls into a BW byte;
	//   - If a 4-bit cell of BW bits crosses a byte boundary and falls into a Color byte, then the last BW bit is repeated until the next color pixel starts.
	//
	// (Tested on Le Chat Mauve IIc adapter, which was made under patent of Video-7)

	UINT32* pDst = (UINT32*)pVideoAddress;

	if (xoffset == 0)	// First cell
	{
		if ((byteval1 & 0x80) || !isMixMode)
		{
			// Color
			
			// Color cell 0
			*(pDst++) = colors[0];
			*(pDst++) = colors[0];
			*(pDst++) = colors[0];
			*(pDst++) = colors[0];
			// Color cell 1
			*(pDst++) = colors[1];
			*(pDst++) = colors[1];
			*(pDst++) = colors[1];
			
			dwordval >>= 7;
			g_dhgrLastCellIsColor = true;
		}
		else
		{
			// BW
			for (int i = 0; i < 7; i++)
			{
				g_dhgrLastBit = dwordval & 1;
				*(pDst++) = bw[g_dhgrLastBit];
				dwordval >>= 1;
			}
			g_dhgrLastCellIsColor = false;
		}

		if ((byteval2 & 0x80) || !isMixMode)
		{
			// Remaining of color cell 1
			if (g_dhgrLastCellIsColor)
			{
				*(pDst++) = colors[1];
			}
			else
			{
				// Repeat last BW bit once
				*(pDst++) = bw[g_dhgrLastBit];
			}
			// Color cell 2
			*(pDst++) = colors[2];
			*(pDst++) = colors[2];
			*(pDst++) = colors[2];
			*(pDst++) = colors[2];
			// Color cell 3
			*(pDst++) = colors[3];
			*(pDst++) = colors[3];
			g_dhgrLastCellIsColor = true;
		}
		else
		{
			for (int i = 0; i < 7; i++)
			{
				g_dhgrLastBit = dwordval & 1;
				*(pDst++) = bw[g_dhgrLastBit];
				dwordval >>= 1;
			}
			g_dhgrLastCellIsColor = false;
		}
	}
	else  // Second cell
	{
		dwordval >>= 14;

		if ((byteval3 & 0x80) || !isMixMode)
		{
			// Remaining of color cell 3
			if (g_dhgrLastCellIsColor)
			{
				*(pDst++) = colors[3];
				*(pDst++) = colors[3];
			}
			else
			{
				// Repeat last BW bit twice
				*(pDst++) = bw[g_dhgrLastBit];
				*(pDst++) = bw[g_dhgrLastBit];
			}
			// Color cell 4
			*(pDst++) = colors[4];
			*(pDst++) = colors[4];
			*(pDst++) = colors[4];
			*(pDst++) = colors[4];
			// Color cell 5
			*(pDst++) = colors[5];
			
			dwordval >>= 7;
			g_dhgrLastCellIsColor = true;
		}
		else
		{
			for (int i = 0; i < 7; i++)
			{
				g_dhgrLastBit = dwordval & 1;
				*(pDst++) = bw[g_dhgrLastBit];
				dwordval >>= 1;
			}
			g_dhgrLastCellIsColor = false;
		}

		if ((byteval4 & 0x80) || !isMixMode)
		{
			// Remaining of color cell 5
			if (g_dhgrLastCellIsColor)
			{
				*(pDst++) = colors[5];
				*(pDst++) = colors[5];
				*(pDst++) = colors[5];
			}
			else
			{
				// Repeat last BW bit three times
				*(pDst++) = bw[g_dhgrLastBit];
				*(pDst++) = bw[g_dhgrLastBit];
				*(pDst++) = bw[g_dhgrLastBit];
			}
			// Color cell 6
			*(pDst++) = colors[6];
			*(pDst++) = colors[6];
			*(pDst++) = colors[6];
			*(pDst++) = colors[6];
			g_dhgrLastCellIsColor = true;
		}
		else
		{
			for (int i = 0; i < 7; i++)
			{
				g_dhgrLastBit = dwordval & 1;
				*(pDst++) = bw[g_dhgrLastBit];
				dwordval >>= 1;
			}
			g_dhgrLastCellIsColor = false;
		}
	}

	const bool bIsHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);

	// Second line
	UINT32* pSrc = (UINT32*)pVideoAddress ;
	pDst = pSrc - GetVideo().GetFrameBufferWidth();
	if (bIsHalfScanLines)
	{
		// Scanlines
		std::fill(pDst, pDst + 14, OPAQUE_BLACK);
	}
	else
	{
		for (int i = 0; i < 14; i++)
			*(pDst + i) = *(pSrc + i);
	}
}

#if 1
// Squash the 640 pixel image into 560 pixels
int UpdateDHiRes160Cell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	const int xpixel = x*16;

	uint8_t *pAux = MemGetAuxPtr(addr);
	uint8_t *pMain = MemGetMainPtr(addr);

	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 = *pAux;
	BYTE byteval3 = *pMain;
	BYTE byteval4 = (x < 39) ? *(pAux+1) : 0;

	DWORD dwordval = (byteval1 & 0xF8)        | ((byteval2 & 0xFF) << 8) |
					((byteval3 & 0xFF) << 16) | ((byteval4 & 0x1F) << 24);
	dwordval <<= 2;

#define PIXEL  0
	CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE), pVideoAddress, 1);
	pVideoAddress += 7;
#undef PIXEL

#define PIXEL  8
	CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE), pVideoAddress, 1);
#undef PIXEL

	return 7*2;
}
#else
// Left align the 640 pixel image, losing the right-hand 80 pixels
int UpdateDHiRes160Cell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	const int xpixel = x*16;
	if (xpixel >= 560)	// clip to our 560px display (losing 80 pixels)
		return 0;

	uint8_t *pAux = MemGetAuxPtr(addr);
	uint8_t *pMain = MemGetMainPtr(addr);

	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 = *pAux;
	BYTE byteval3 = *pMain;
	BYTE byteval4 = (x < 39) ? *(pAux+1) : 0;

	DWORD dwordval = (byteval1 & 0xFC)        | ((byteval2 & 0xFF) << 8) |	// NB. Needs more bits than above squashed version, to avoid vertical black lines
					((byteval3 & 0xFF) << 16) | ((byteval4 & 0x3F) << 24);
	dwordval <<= 2;

#define PIXEL  0
	CopySource(8,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE), pVideoAddress);
	pVideoAddress += 8;
#undef PIXEL

#define PIXEL  8
	CopySource(8,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE), pVideoAddress);
#undef PIXEL

	return 8*2;
}
#endif

//===========================================================================

// Tested with Deater's Cycle-Counting Megademo
void UpdateLoResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	const BYTE val = *MemGetMainPtr(addr);

	if ((y & 4) == 0)
	{
		CopySource(14,2, SRCOFFS_LORES+((x & 1) << 1), ((val & 0xF) << 4), pVideoAddress);
	}
	else
	{
		CopySource(14,2, SRCOFFS_LORES+((x & 1) << 1), (val & 0xF0), pVideoAddress);
	}
}

//===========================================================================

#define ROL_NIB(x) ( (((x)<<1)&0xF) | (((x)>>3)&1) )

// Tested with FT's Ansi Story
void UpdateDLoResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	BYTE auxval = *MemGetAuxPtr(addr);
	const BYTE mainval = *MemGetMainPtr(addr);

	const BYTE auxval_h = auxval >> 4;
	const BYTE auxval_l = auxval & 0xF;
	auxval = (ROL_NIB(auxval_h)<<4) | ROL_NIB(auxval_l);

	if ((y & 4) == 0)
	{
		CopySource(7,2, SRCOFFS_LORES+((x & 1) << 1), ((auxval & 0xF) << 4), pVideoAddress);
		CopySource(7,2, SRCOFFS_LORES+((x & 1) << 1), ((mainval & 0xF) << 4), pVideoAddress+7);
	}
	else
	{
		CopySource(7,2, SRCOFFS_LORES+((x & 1) << 1), (auxval & 0xF0), pVideoAddress);
		CopySource(7,2, SRCOFFS_LORES+((x & 1) << 1), (mainval & 0xF0), pVideoAddress+7);
	}
}

//===========================================================================
// Color TEXT (some RGB cards only)
// Default BG and FG are usually defined by hardware switches, defaults to black/white
void UpdateText40ColorCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, uint8_t bits, uint8_t character)
{
	uint8_t foreground = g_nRegularTextFG;
	uint8_t background = g_nRegularTextBG;
	if (g_nTextFBMode)
	{
		const BYTE val = *MemGetAuxPtr(addr);  // RGB cards with F/B text use their own AUX memory!
		foreground = val >> 4;
		background = val & 0x0F;
	}
	else if (g_RGBVideocard == RGB_Videocard_e::Video7_SL7 && character < 0x80)
	{
		// in regular 40COL mode, the SL7 videocard renders inverse characters as B&W
		foreground = 15;
		background = 0;
	}

	UpdateDuochromeCell(2, 14, pVideoAddress, bits, foreground, background);
}

void UpdateText80ColorCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, uint8_t bits, uint8_t character)
{
	if (g_RGBVideocard == RGB_Videocard_e::Video7_SL7 && character < 0x80)
	{
		// in all 80COL modes, the SL7 videocard renders inverse characters as B&W
		UpdateDuochromeCell(2, 7, pVideoAddress, bits, 15, 0);
	}
	else
	{
		UpdateDuochromeCell(2, 7, pVideoAddress, bits, g_nRegularTextFG, g_nRegularTextBG);
	}
}

//===========================================================================
// Duochrome HGR (some RGB cards only)
void UpdateHiResDuochromeCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress)
{
	BYTE bits = *MemGetMainPtr(addr);
	BYTE val = *MemGetAuxPtr(addr);
	const uint8_t foreground = val >> 4;
	const uint8_t background = val & 0x0F;

	UpdateDuochromeCell(2, 14, pVideoAddress, bits, foreground, background);
}

//===========================================================================
// Writes a duochrome cell
// 7 bits define a foreground/background pattern
// Used on many RGB cards but activated differently, depending on the card.
// Can be used in TEXT or HGR mode. The foreground & background colors could be fixed by hardware switches or data lying in AUX.
void UpdateDuochromeCell(int h, int w, bgra_t* pVideoAddress, uint8_t bits, uint8_t foreground, uint8_t background)
{
	UINT32* pDst = (UINT32*)pVideoAddress;

	const bool bIsHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);
	const UINT frameBufferWidth = GetVideo().GetFrameBufferWidth();
	RGBQUAD colors[2];
	// use LoRes palette
	background += 12;
	foreground += 12;
	// get bg/fg colors
	colors[0] = g_pPaletteRGB[background];
	colors[1] = g_pPaletteRGB[foreground];
	int nbits = bits;
	int doublepixels = (w == 14); // Double pixel (HiRes or Text40)

	while (h--)
	{
		bits = nbits;
		if (bIsHalfScanLines && !(h & 1))
		{
			// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
			std::fill(pDst, pDst + w, OPAQUE_BLACK);
		}
		else
		{
			for (int nBytes = 0; nBytes < w; nBytes += (doublepixels?2:1))
			{
				int bit = (bits & 1);
				bits >>= 1;
				const RGBQUAD& rRGB = colors[bit];
				*(pDst + nBytes) = *reinterpret_cast<const UINT32*>(&rRGB);
				if (doublepixels)
				{
					*(pDst + nBytes + 1) = *reinterpret_cast<const UINT32*>(&rRGB);
				}
			}
		}

		pDst -= frameBufferWidth;
	}
}

//===========================================================================

static LPBYTE g_pSourcePixels = NULL;

static void V_CreateDIBSections(void)
{
	if (!g_pSourcePixels)	// NB. Will be non-zero after a VM restart (GH#809)
		g_pSourcePixels = new BYTE[SRCOFFS_TOTAL * MAX_SOURCE_Y];

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE SOURCE IMAGE
	for (int y = 0; y < MAX_SOURCE_Y; y++)
		g_aSourceStartofLine[ y ] = g_pSourcePixels + SRCOFFS_TOTAL*((MAX_SOURCE_Y-1) - y);

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	memset(g_pSourcePixels, 0, SRCOFFS_TOTAL*MAX_SOURCE_Y);

	V_CreateLookup_Lores();
	V_CreateLookup_HiResHalfPixel_Authentic(VT_COLOR_IDEALIZED);
	V_CreateLookup_DoubleHires();

	CreateColorMixMap();
}

void VideoInitializeOriginal(baseColors_t pBaseNtscColors)
{
	// CREATE THE SOURCE IMAGE AND DRAW INTO THE SOURCE BIT BUFFER
	V_CreateDIBSections();

	// Replace the default palette with true NTSC-generated colors
	memcpy(&PaletteRGB_NTSC[BLACK], *pBaseNtscColors, sizeof(RGBQUAD) * kNumBaseColors);
	PaletteRGB_NTSC[HGR_BLUE]   = PaletteRGB_NTSC[BLUE];
	PaletteRGB_NTSC[HGR_ORANGE] = PaletteRGB_NTSC[ORANGE];
	PaletteRGB_NTSC[HGR_GREEN]  = PaletteRGB_NTSC[GREEN];
	PaletteRGB_NTSC[HGR_VIOLET] = PaletteRGB_NTSC[MAGENTA];
}

//===========================================================================

// RGB videocards may use a different palette thant the NTSC-generated one
void VideoSwitchVideocardPalette(RGB_Videocard_e videocard, VideoType_e type)
{
	g_pPaletteRGB = PaletteRGB_NTSC;
	if (type==VideoType_e::VT_COLOR_VIDEOCARD_RGB && videocard == RGB_Videocard_e::LeChatMauve_Feline)
	{
		g_pPaletteRGB = PaletteRGB_Feline;
	}
}

//===========================================================================

static UINT g_rgbFlags = 0;
static UINT g_rgbMode = 0;
static WORD g_rgbPrevAN3Addr = 0;
static bool g_rgbInvertBit7 = false;

// Video7 RGB card:
// . Clock in the !80COL state to define the 2 flags: F2, F1
// . Clocking done by toggling AN3
// . NB. There's a final 5th AN3 transition to set DHGR mode
void RGB_SetVideoMode(WORD address)
{
	if ((address & ~1) != 0x5E)			// 0x5E or 0x5F? (DHIRES)
		return;

	// Precondition before toggling AN3:
	// . Video7 manual: set 80STORE, but "King's Quest 1"(*) will re-enable RGB card's MIX mode with only VF_TEXT & VF_HIRES set!
	// . "Extended 80-Column Text/AppleColor Card" manual: TEXT off($C050), MIXED off($C052), HIRES on($C057)
	// . (*) "King's Quest 1" - see routine at 0x5FD7 (trigger by pressing TAB twice)
	// . Apple II desktop sets DHGR B&W mode with HIRES off! (GH#631)
	// Maybe there is no video-mode precondition?
	// . After setting 80COL on/off then need a 0x5E->0x5F toggle. So if we see a 0x5F then reset (GH#633)

	// From Video7 patent and Le Chat Mauve manuals (under patent of Video7), no precondition is needed.

	// In Prince of Persia, in the game demo, the RGB card switches to BW DHIRES after the HGR animation with Jaffar.
	// It's actually the same on real hardware (tested on IIc RGB adapter).

	if (address == 0x5F && g_rgbPrevAN3Addr == 0x5E)
	{
		g_rgbFlags = (g_rgbFlags << 1) & 3;
		g_rgbFlags |= ((GetVideo().GetVideoMode() & VF_80COL) ? 0 : 1);	// clock in !80COL
		g_rgbMode = g_rgbFlags;								// latch F2,F1
	}

	g_rgbPrevAN3Addr = address;
}

bool RGB_Is140Mode(void)	// Extended 80-Column Text/AppleColor Card's Mode 2
{
	// Feline falls back to this mode instead of 160
	return g_rgbMode == 0 || (g_RGBVideocard == RGB_Videocard_e::LeChatMauve_Feline && g_rgbMode == 1);
}

bool RGB_Is160Mode(void)	// Extended 80-Column Text/AppleColor Card: N/A
{
	// Unsupported by Feline
	return g_rgbMode == 1 && (g_RGBVideocard != RGB_Videocard_e::LeChatMauve_Feline);
}

bool RGB_IsMixMode(void)	// Extended 80-Column Text/AppleColor Card's Mode 3
{
	return g_rgbMode == 2;
}

bool RGB_Is560Mode(void)	// Extended 80-Column Text/AppleColor Card's Mode 1
{
	return g_rgbMode == 3;
}

bool RGB_IsMixModeInvertBit7(void)
{
	return RGB_IsMixMode() && g_rgbInvertBit7;
}

void RGB_ResetState(void)
{
	g_rgbFlags = 0;
	g_rgbMode = 0;
	g_rgbPrevAN3Addr = 0;
}

void RGB_SetInvertBit7(bool state)
{
	g_rgbInvertBit7 = state;
}

//===========================================================================

#define SS_YAML_KEY_RGB_CARD "AppleColor RGB Adaptor"
// NB. No version - this is determined by the parent card

#define SS_YAML_KEY_RGB_FLAGS "RGB mode flags"
#define SS_YAML_KEY_RGB_MODE "RGB mode"
#define SS_YAML_KEY_RGB_PREVIOUS_AN3 "Previous AN3"
#define SS_YAML_KEY_RGB_80COL_CHANGED "80COL changed"
#define SS_YAML_KEY_RGB_INVERT_BIT7 "Invert bit7"

void RGB_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_RGB_CARD);

	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_FLAGS, g_rgbFlags);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_MODE, g_rgbMode);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_PREVIOUS_AN3, g_rgbPrevAN3Addr);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_RGB_80COL_CHANGED, false);	// unused (todo: remove next time the parent card's version changes)
	yamlSaveHelper.SaveBool(SS_YAML_KEY_RGB_INVERT_BIT7, g_rgbInvertBit7);
}

void RGB_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT cardVersion)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_RGB_CARD))
		throw std::string("Card: Expected key: ") + std::string(SS_YAML_KEY_RGB_CARD);

	g_rgbFlags = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_FLAGS);
	g_rgbMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_MODE);
	g_rgbPrevAN3Addr = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_PREVIOUS_AN3);

	if (cardVersion >= 3)
	{
		yamlLoadHelper.LoadBool(SS_YAML_KEY_RGB_80COL_CHANGED);	// Obsolete (so just consume)
		g_rgbInvertBit7 = yamlLoadHelper.LoadBool(SS_YAML_KEY_RGB_INVERT_BIT7);
	}

	yamlLoadHelper.PopMap();
}

RGB_Videocard_e RGB_GetVideocard(void)
{
	return g_RGBVideocard;
}

void RGB_SetVideocard(RGB_Videocard_e videocard, int text_foreground, int text_background)
{
	g_RGBVideocard = videocard;

	// black & white text
	RGB_SetRegularTextFG(15);
	RGB_SetRegularTextBG(0);

	if (videocard == RGB_Videocard_e::Video7_SL7 &&
		(text_foreground == 6 || text_foreground == 9 || text_foreground == 12 || text_foreground == 15))
	{
		// SL7: Only Blue, Amber (Orange), Green, White are supported by hardware switches
		RGB_SetRegularTextFG(text_foreground);
		RGB_SetRegularTextBG(0);
	}
}

void RGB_SetRegularTextFG(int color)
{
	g_nRegularTextFG = color;
}

void RGB_SetRegularTextBG(int color)
{
	g_nRegularTextBG = color;
}

void RGB_EnableTextFB()
{
	g_nTextFBMode = 1;
}

void RGB_DisableTextFB()
{
	g_nTextFBMode = 0;
}

int RGB_IsTextFB()
{
	return g_nTextFBMode;
}
