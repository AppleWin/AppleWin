// Sync'd with 1.25.0.4 source

#include "StdAfx.h"

#include "Frame.h"
#include "Memory.h" // MemGetMainPtr() MemGetAuxPtr()
#include "Video.h"
#include "RGBMonitor.h"
#include "YamlHelper.h"

const int SRCOFFS_LORES   = 0;                       //    0
const int SRCOFFS_HIRES   = (SRCOFFS_LORES  +   16); //   16
const int SRCOFFS_HIRES2  = (SRCOFFS_HIRES  +  512); //  528		// Style = Vertical Blend, 280-pixel (from 1.25)
const int SRCOFFS_DHIRES  = (SRCOFFS_HIRES2 +  512); // 1040
const int SRCOFFS_TOTAL   = (SRCOFFS_DHIRES + 2560); // 3600

const int MAX_SOURCE_Y = 512;
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

#define  SETRGBCOLOR(r,g,b) {b,g,r,0}

static RGBQUAD PalIndex2RGB[] =
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

//===========================================================================

static void V_CreateLookup_DoubleHires ()
{
#define OFFSET  3
#define SIZE    10

  for (int column = 0; column < 256; column++) {
    int coloffs = SIZE * column;
    for (unsigned byteval = 0; byteval < 256; byteval++) {
      int color[SIZE];
      ZeroMemory(color,sizeof(color));
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

      int y = byteval << 1;
      for (int x = 0; x < SIZE; x++) {
        SETSOURCEPIXEL(SRCOFFS_DHIRES+coloffs+x,y  ,DoubleHiresPalIndex[ color[x] ]);
        SETSOURCEPIXEL(SRCOFFS_DHIRES+coloffs+x,y+1,DoubleHiresPalIndex[ color[x] ]);
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

#define HALF_PIXEL_SOLID 1
#define HALF_PIXEL_BLEED 0

void V_CreateLookup_HiResHalfPixel_Authentic(VideoType_e videoType) // Colors are solid (100% coverage)
{
	// 2-bits from previous byte, 2-bits from next byte = 2^4 = 16 total permutations
	for (int iColumn = 0; iColumn < 16; iColumn++)
	{
		int offsetx = iColumn << 5; // every column is 32 bytes wide -- 7 apple pixels = 14 pixels + 2 pad + 14 pixels + 2 pad

		for (unsigned iByte = 0; iByte < 256; iByte++)
		{
			int aPixels[11]; // c2 c1 b7 b6 b5 b4 b3 b2 b1 b0 c8 c4

/*
aPixel[i]
 A 9|8 7 6 5 4 3 2|1 0
 Z W|b b b b b b b|X Y
----+-------------+----
prev|  existing   |next
bits| hi-res byte |bits

Legend:
 XYZW = iColumn in binary
 b = Bytes in binary
*/
			// aPixel[] = 48bbbbbbbb12, where b = iByte in binary, # is bit-n of column
			aPixels[ 0] = iColumn & 4; // previous byte, 2nd last pixel
			aPixels[ 1] = iColumn & 8; // previous byte, last pixel
			aPixels[ 9] = iColumn & 1; // next byte, first pixel
			aPixels[10] = iColumn & 2; // next byte, second pixel

			// Convert raw pixel Byte value to binary and stuff into bit array of pixels on off
			int nBitMask = 1;
			int iPixel;
			for (iPixel  = 2; iPixel < 9; iPixel++)
			{
				aPixels[iPixel] = ((iByte & nBitMask) != 0);
				nBitMask <<= 1;
			}

			int hibit = (iByte >> 7) & 1; // ((iByte & 0x80) != 0);
			int x     = 0;
			int y     = iByte << 1;

/* Test cases
 81 blue
   2000:D5 AA D5 AA
 82 orange
   2800:AA D5 AA D5
 FF white bleed "thru"
   3000:7F 80 7F 80
   3800:FF 80 FF 80
   2028:80 7F 80 7F
   2828:80 FF 80 FF
 Edge Case for Half Luminance !
   2000:C4 00  // Green  HalfLumBlue
   2400:C4 80  // Green  Green
 Edge Case for Color Bleed !
   2000:40 00
   2400:40 80
 Nox Archaist (GH#616)
   2000:00 40 9E  // Green Black White
*/

			// Fixup missing pixels that normally have been scan-line shifted -- Apple "half-pixel" -- but cross 14-pixel boundaries.
			if( hibit )
			{
				if ( aPixels[1] ) // preceding pixel on?
#if 0 // Optimization: Doesn't seem to matter if we ignore the 2 pixels of the next byte
					for (iPixel = 0; iPixel < 9; iPixel++) // NOTE: You MUST start with the preceding 2 pixels !!!
						if (aPixels[iPixel]) // pixel on
#endif
						{
							if (aPixels[2] || aPixels[0]) // White if pixel from previous byte and first pixel of this byte is on
							{
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , HGR_WHITE );
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y+1, HGR_WHITE );
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , HGR_WHITE );
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, HGR_WHITE );
							} else {   // Optimization:   odd = (iPixel & 1); if (!odd) case is same as if(odd) !!! // Reference: Gumball - Gumball Machine
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , HGR_ORANGE ); // left half of orange pixels 
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y+1, HGR_ORANGE );
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , HGR_BLUE ); // right half of blue pixels 4, 11, 18, ...
								SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, HGR_BLUE );
							}
						}
#if HALF_PIXEL_SOLID
// Test Patterns 
// 81 blue
//   2000:D5 AA D5 AA -> 2001:AA D5  should not have black gap, should be blue
// 82 orange
//   2800:AA D5 AA D5
// Game: Elite -- Loading Logo 
//   2444:BB F7 -> 2000:BB F7    // Should not have orange in-between gap -- Elite "Firebird" Logo
//              -> 2400:00 BB F7 // Should not have blue in-between gap )
//   21D0:C0 00    -> HalfLumBlue
//   25D0:C0 D0 88 -> Blue black orange black orange
//   29D0:C0 90 88 -> Blue black orange
// Game: Ultima 4 -- Ultima 4 Logo - bottom half of screen has a "mini-game" / demo -- far right has tree and blue border
//   2176:2A AB green black_gap white blue_border // Should have black gap between green and white
				else if ( aPixels[0] ) // prev prev pixel on
				{
// Game: Gumball
//   218E:AA 97    => 2000: A9 87          orange_white            // Should have no gap between orange and white
//   229A:AB A9 87 -> 2000: 00 A9 87 white orange black blue_white // Should have no gap between blue and white
//   2001:BB F7                            white blue white  (Gumball Intermission)
// Torture Half-Pixel HGR Tests:  This is a real bitch to solve -- we really need to check:
//     if (hibit_prev_byte && !aPixels[iPixel-3] && aPixels[iPixel-2] && !aPixels[iPixel] && hibit_this_byte) then set first half-pixel of this byte to either blue or orange
//   2000:A9 87 halfblack blue black black orange black orange black
//   2400:BB F7 halfblack white white black white white white halfblack
//  or
//   2000:A0 83 orange should "bleed" thru
//   2400:B0 83 should have black gap

					if ( aPixels[2] )
#if HALF_PIXEL_BLEED // No Half-Pixel Bleed
						if ( aPixels[3] ) {
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , DARK_BLUE ); // Gumball: 229A: AB A9 87
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y+1, DARK_BLUE );
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , BROWN ); // half luminance red Elite: 2444: BB F7
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, BROWN ); // half luminance red Gumball: 218E: AA 97
						} else {
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , HGR_BLUE ); // 2000:D5 AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y+1, HGR_BLUE );
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , HGR_ORANGE ); // 2000: AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, HGR_ORANGE );
						}
#else
						if ((videoType == VT_COLOR_MONITOR_RGB) || ( !aPixels[3] ))
						{ // "Text optimized" IF this pixel on, and adjacent right pixel off, then colorize first half-pixel of this byte
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y  , HGR_BLUE ); // 2000:D5 AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+0 ,y+1, HGR_BLUE );
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y  , HGR_ORANGE ); // 2000: AA D5
							SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+16,y+1, HGR_ORANGE );
						}
#endif // HALF_PIXEL_BLEED
				}
#endif // HALF_PIXEL_SOLID
			}
			x += hibit;

			while (x < 28)
			{
				int adj = (x >= 14) << 1; // Adjust start of 7 last pixels to be 16-byte aligned!
				int odd = (x >= 14);
				for (iPixel = 2; iPixel < 9; iPixel++)
				{
					int color = CM_Black;
					if (aPixels[iPixel]) // pixel on
					{
						color = CM_White; 
						if (aPixels[iPixel-1] || aPixels[iPixel+1]) // adjacent pixels are always white
							color = CM_White; 
						else
							color = ((odd ^ (iPixel&1)) << 1) | hibit; // map raw color to our hi-res colors
					}
#if HALF_PIXEL_SOLID
					else if (aPixels[iPixel-1] && aPixels[iPixel+1]) // IF prev_pixel && next_pixel THEN
					{
						// Activate fringe reduction on white HGR text - drawback: loss of color mix patterns in HGR video mode.
						if (
							(videoType == VT_COLOR_MONITOR_RGB) // Fill in colors in between white pixels
//						||	(videoType == VT_COLOR_TVEMU)       // Fill in colors in between white pixels (Post Processing will mix/merge colors)
						|| !(aPixels[iPixel-2] && aPixels[iPixel+2]) ) // VT_COLOR_TEXT_OPTIMIZED -> Don't fill in colors in between white
						{
							// Test Pattern: Ultima 4 Logo - Castle
							// 3AC8: 36 5B 6D 36
							color = ((odd ^ !(iPixel&1)) << 1) | hibit;	// No white HGR text optimization
						}
					}
#endif
					// Colors - Top/Bottom Left/Right
					// cTL cTR
					// cBL cBR
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+adj  ,y  ,HiresToPalIndex[color]); // cTL
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+adj+1,y  ,HiresToPalIndex[color]); // cTR
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+adj  ,y+1,HiresToPalIndex[color]); // cBL
					SETSOURCEPIXEL(SRCOFFS_HIRES+offsetx+x+adj+1,y+1,HiresToPalIndex[color]); // cBR
					x += 2;
				}
			}
		}
	}
}

//===========================================================================

void V_CreateLookup_Hires()
{
//	int iMonochrome = GetMonochromeIndex();

	// BYTE colorval[6] = {MAGENTA,BLUE,GREEN,ORANGE,BLACK,WHITE};
	// BYTE colorval[6] = {HGR_VIOLET,HGR_BLUE,HGR_GREEN,HGR_ORANGE,HGR_BLACK,HGR_WHITE};
	for (int iColumn = 0; iColumn < 16; iColumn++)
	{
		int coloffs = iColumn << 5;

		for (unsigned iByte = 0; iByte < 256; iByte++)
		{
			int aPixels[11];

			aPixels[ 0] = iColumn & 4;
			aPixels[ 1] = iColumn & 8;
			aPixels[ 9] = iColumn & 1;
			aPixels[10] = iColumn & 2;

			int nBitMask = 1;
			int iPixel;
			for (iPixel  = 2; iPixel < 9; iPixel++) {
				aPixels[iPixel] = ((iByte & nBitMask) != 0);
				nBitMask <<= 1;
			}

			int hibit = ((iByte & 0x80) != 0);
			int x     = 0;
			int y     = iByte << 1;

			while (x < 28)
			{
				int adj = (x >= 14) << 1;
				int odd = (x >= 14);

				for (iPixel = 2; iPixel < 9; iPixel++)
				{
					int color = CM_Black;
					if (aPixels[iPixel])
					{
						if (aPixels[iPixel-1] || aPixels[iPixel+1])
							color = CM_White;
						else
							color = ((odd ^ (iPixel&1)) << 1) | hibit;
					}
					else if (aPixels[iPixel-1] && aPixels[iPixel+1])
					{
						// Activate fringe reduction on white HGR text - drawback: loss of color mix patterns in HGR video mode.
						// VT_COLOR_STANDARD = Fill in colors in between white pixels
						// VT_COLOR_TVEMU    = Fill in colors in between white pixels  (Post Processing will mix/merge colors)
						// VT_COLOR_TEXT_OPTIMIZED --> !(aPixels[iPixel-2] && aPixels[iPixel+2]) = Don't fill in colors in between white
						if (/*(g_eVideoType == VT_COLOR_TVEMU) ||*/ !(aPixels[iPixel-2] && aPixels[iPixel+2]) )
							color = ((odd ^ !(iPixel&1)) << 1) | hibit;	// No white HGR text optimization
					}

					//if (g_eVideoType == VT_MONO_AUTHENTIC) {
					//	int nMonoColor = (color != CM_Black) ? iMonochrome : BLACK;
					//	SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj  ,y  , nMonoColor); // buggy
					//	SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj+1,y  , nMonoColor); // buggy
					//	SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj  ,y+1,BLACK); // BL
					//	SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj+1,y+1,BLACK); // BR
					//} else
					{
						// Colors - Top/Bottom Left/Right
						// cTL cTR
						// cBL cBR
						SETSOURCEPIXEL(SRCOFFS_HIRES2+coloffs+x+adj  ,y  ,HiresToPalIndex[color]); // cTL
						SETSOURCEPIXEL(SRCOFFS_HIRES2+coloffs+x+adj+1,y  ,HiresToPalIndex[color]); // cTR
						SETSOURCEPIXEL(SRCOFFS_HIRES2+coloffs+x+adj  ,y+1,HiresToPalIndex[color]); // cBL
						SETSOURCEPIXEL(SRCOFFS_HIRES2+coloffs+x+adj+1,y+1,HiresToPalIndex[color]); // cBR
					}
					x += 2;
				}
			}
		}
	}
}

//===========================================================================

// For AppleWin 1.25 "tv emulation" HGR Video Mode

const UINT FRAMEBUFFER_W = 560;
const UINT FRAMEBUFFER_H = 384;
const UINT HGR_MATRIX_YOFFSET = 2;

static BYTE hgrpixelmatrix[FRAMEBUFFER_W][FRAMEBUFFER_H/2 + 2 * HGR_MATRIX_YOFFSET];	// 2 extra scan lines on bottom?
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

static void MixColorsVertical(int matx, int maty)
{
	int bot1idx, bot2idx;

	if (VideoGetSWMIXED() && maty > 159)
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

static void CopyMixedSource(int x, int y, int sourcex, int sourcey, bgra_t *pVideoAddress)
{
	const BYTE* const currsourceptr = g_aSourceStartofLine[sourcey]+sourcex;
	    UINT32* const currdestptr   = (UINT32*) pVideoAddress;

	const int matx = x*2;
	const int maty = HGR_MATRIX_YOFFSET + y;
	const int hgrlinesabove = (y > 0) ? 1 : 0;
	const int hgrlinesbelow = VideoGetSWMIXED() ? ((y < 159)? 1:0) : ((y < 191)? 1:0);
	const int istart        = 2 - (hgrlinesabove*2);
	const int iend          = 3 + (hgrlinesbelow*2);

	// transfer 14 pixels (i.e. the visible part of an apple hgr-byte) from row to pixelmatrix
	for (int count = 0, bufxoffset = 0; count < 14; count++, bufxoffset += 1)
	{
		hgrpixelmatrix[matx+count][maty] = *(currsourceptr+bufxoffset);

		// color mixing between adjacent scanlines at current x position
		MixColorsVertical(matx+count, maty);

		// transfer up to 6 mixed (half-)pixels of current column to framebuffer
		UINT32* currptr = currdestptr+bufxoffset;
		if (hgrlinesabove)
			currptr += GetFrameBufferWidth() * 2;

		for (int i = istart; i <= iend; currptr -= GetFrameBufferWidth(), i++)
		{
			if (IsVideoStyle(VS_HALF_SCANLINES) && (i & 1))
			{
				// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
				*currptr = 0;
			}
			else
			{
				_ASSERT( colormixbuffer[i] < (sizeof(PalIndex2RGB)/sizeof(PalIndex2RGB[0])) );
				const RGBQUAD& rRGB = PalIndex2RGB[ colormixbuffer[i] ];
				const UINT32 rgb = (((UINT32)rRGB.rgbRed)<<16) | (((UINT32)rRGB.rgbGreen)<<8) | ((UINT32)rRGB.rgbBlue);
				*currptr = rgb;
			}
		}
	}
}

//===========================================================================

// Pre: nSrcAdjustment: for 160-color images, src is +1 compared to dst
static void CopySource(int w, int h, int sx, int sy, bgra_t *pVideoAddress, const int nSrcAdjustment = 0)
{
	UINT32* pDst = (UINT32*) pVideoAddress;
	LPBYTE pSrc = g_aSourceStartofLine[ sy ] + sx;
	int nBytes;

	while (h--)
	{
		nBytes = w;
		while (nBytes)
		{
			--nBytes;

			if (IsVideoStyle(VS_HALF_SCANLINES) && !(h & 1))
			{
				// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
				*(pDst+nBytes) = 0;
			}
			else
			{
				_ASSERT( *(pSrc+nBytes+nSrcAdjustment) < (sizeof(PalIndex2RGB)/sizeof(PalIndex2RGB[0])) );
				const RGBQUAD& rRGB = PalIndex2RGB[ *(pSrc+nBytes+nSrcAdjustment) ];
				const UINT32 rgb = (((UINT32)rRGB.rgbRed)<<16) | (((UINT32)rRGB.rgbGreen)<<8) | ((UINT32)rRGB.rgbBlue);
				*(pDst+nBytes) = rgb;
			}
		}

		pDst -= GetFrameBufferWidth();
		pSrc -= SRCOFFS_TOTAL;
	}
}

//===========================================================================

void UpdateHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	uint8_t *pMain = MemGetMainPtr(addr);
	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 =            *(pMain);
	BYTE byteval3 = (x < 39) ? *(pMain+1) : 0;

#define COLOFFS  (((byteval1 & 0x60) << 2) | ((byteval3 & 0x03) << 5))
	if (IsVideoStyle(VS_COLOR_VERTICAL_BLEND))
	{
		CopyMixedSource(
			x*7, y,
//			SRCOFFS_HIRES2+COLOFFS+((x & 1) << 4), (((int)byteval2) << 1),
			SRCOFFS_HIRES+COLOFFS+((x & 1) << 4), (((int)byteval2) << 1),
			pVideoAddress
		);
	}
	else
	{
		CopySource(14,2, SRCOFFS_HIRES+COLOFFS+((x & 1) << 4), (((int)byteval2) << 1), pVideoAddress);
	}
#undef COLOFFS
}

//===========================================================================

#define COLOR  ((xpixel + PIXEL) & 3)
#define VALUE  (dwordval >> (4 + PIXEL - COLOR))

void UpdateDHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress, bool updateAux, bool updateMain)
{
	const int xpixel = x*14;

	uint8_t *pAux = MemGetAuxPtr(addr);
	uint8_t *pMain = MemGetMainPtr(addr);

	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 = *pAux;
	BYTE byteval3 = *pMain;
	BYTE byteval4 = (x < 39) ? *(pAux+1) : 0;

	DWORD dwordval = (byteval1 & 0x70)        | ((byteval2 & 0x7F) << 7) |
					((byteval3 & 0x7F) << 14) | ((byteval4 & 0x07) << 21);

#define PIXEL  0
	if (updateAux)
	{
		CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress);
		pVideoAddress += 7;
	}
#undef PIXEL

#define PIXEL  7
	if (updateMain)
	{
		CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress);
	}
#undef PIXEL
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
	CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress, 1);
	pVideoAddress += 7;
#undef PIXEL

#define PIXEL  8
	CopySource(7,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress, 1);
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
	CopySource(8,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress);
	pVideoAddress += 8;
#undef PIXEL

#define PIXEL  8
	CopySource(8,2, SRCOFFS_DHIRES+10*HIBYTE(VALUE)+COLOR, LOBYTE(VALUE)<<1, pVideoAddress);
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

static LPBYTE g_pSourcePixels = NULL;

static void V_CreateDIBSections(void)
{
	g_pSourcePixels = new BYTE[SRCOFFS_TOTAL * MAX_SOURCE_Y];

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE SOURCE IMAGE
	for (int y = 0; y < MAX_SOURCE_Y; y++)
		g_aSourceStartofLine[ y ] = g_pSourcePixels + SRCOFFS_TOTAL*((MAX_SOURCE_Y-1) - y);

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	ZeroMemory(g_pSourcePixels, SRCOFFS_TOTAL*MAX_SOURCE_Y); // 32 bytes/pixel * 16 colors = 512 bytes/row

	V_CreateLookup_Lores();
//	V_CreateLookup_Hires();	// For CopyMixedSource() / VS_COLOR_VERTICAL_BLEND, 280-pixel (from 1.25)
	V_CreateLookup_HiResHalfPixel_Authentic(VT_COLOR_MONITOR_RGB);
	V_CreateLookup_DoubleHires();

	CreateColorMixMap();
}

void VideoInitializeOriginal(baseColors_t pBaseNtscColors)
{
	// CREATE THE SOURCE IMAGE AND DRAW INTO THE SOURCE BIT BUFFER
	V_CreateDIBSections();

	memcpy(&PalIndex2RGB[BLACK], *pBaseNtscColors, sizeof(RGBQUAD)*kNumBaseColors);
	PalIndex2RGB[HGR_BLUE]   = PalIndex2RGB[BLUE];
	PalIndex2RGB[HGR_ORANGE] = PalIndex2RGB[ORANGE];
	PalIndex2RGB[HGR_GREEN]  = PalIndex2RGB[GREEN];
	PalIndex2RGB[HGR_VIOLET] = PalIndex2RGB[MAGENTA];
}

//===========================================================================

static UINT g_rgbFlags = 0;
static UINT g_rgbMode = 0;
static WORD g_rgbPrevAN3Addr = 0;

// Video7 RGB card:
// . Clock in the !80COL state to define the 2 flags: F2, F1
// . Clocking done by toggling AN3
// . NB. There's a final 5th AN3 transition to set DHGR mode
void RGB_SetVideoMode(WORD address)
{
	if ((address&~1) != 0x5E)			// 0x5E or 0x5F?
		return;

	// Precondition before toggling AN3:
	// . Video7 manual: set 80STORE, but "King's Quest 1"(*) will re-enable RGB card's MIX mode with only VF_TEXT & VF_HIRES set!
	// . "Extended 80-Column Text/AppleColor Card" manual: TEXT off($C050), MIXED off($C052), HIRES on($C057)
	// . (*) "King's Quest 1" - see routine at 0x5FD7 (trigger by pressing TAB twice)
	if ((g_uVideoMode & (VF_MIXED|VF_HIRES)) != (VF_HIRES))
	{
		g_rgbMode = 0;
		g_rgbPrevAN3Addr = 0;
		return;
	}

	if (address == 0x5F && g_rgbPrevAN3Addr == 0x5E)	// Check for AN3 clock transition
	{
		g_rgbFlags = (g_rgbFlags<<1) & 3;
		g_rgbFlags |= ((g_uVideoMode & VF_80COL) ? 0 : 1);	// clock in !80COL
		g_rgbMode = g_rgbFlags;								// latch F2,F1
	}

	g_rgbPrevAN3Addr = address;
}

bool RGB_Is140Mode(void)	// Extended 80-Column Text/AppleColor Card's Mode 2
{
	return g_rgbMode == 0;
}

bool RGB_Is160Mode(void)	// Extended 80-Column Text/AppleColor Card: N/A
{
	return g_rgbMode == 1;
}

bool RGB_IsMixMode(void)	// Extended 80-Column Text/AppleColor Card's Mode 3
{
	return g_rgbMode == 2;
}

bool RGB_Is560Mode(void)	// Extended 80-Column Text/AppleColor Card's Mode 1
{
	return g_rgbMode == 3;
}

void RGB_ResetState(void)
{
	g_rgbFlags = 0;
	g_rgbMode = 0;
	g_rgbPrevAN3Addr = 0;
}

//===========================================================================

#define SS_YAML_KEY_RGB_CARD "AppleColor RGB Adaptor"
// NB. No version - this is determined by the parent card

#define SS_YAML_KEY_RGB_FLAGS "RGB mode flags"
#define SS_YAML_KEY_RGB_MODE "RGB mode"
#define SS_YAML_KEY_RGB_PREVIOUS_AN3 "Previous AN3"

void RGB_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", SS_YAML_KEY_RGB_CARD);

	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_FLAGS, g_rgbFlags);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_MODE, g_rgbMode);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_RGB_PREVIOUS_AN3, g_rgbPrevAN3Addr);
}

void RGB_LoadSnapshot(YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_RGB_CARD))
		throw std::string("Card: Expected key: ") + std::string(SS_YAML_KEY_RGB_CARD);

	g_rgbFlags = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_FLAGS);
	g_rgbMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_MODE);
	g_rgbPrevAN3Addr = yamlLoadHelper.LoadUint(SS_YAML_KEY_RGB_PREVIOUS_AN3);

	yamlLoadHelper.PopMap();
}
