// Sync'd with 1.25.0.4 source

#include "StdAfx.h"

#include "Frame.h"
#include "Memory.h" // MemGetMainPtr() MemGetAuxPtr()
#include "Video.h"

const int SRCOFFS_40COL   = 0;                       //    0
const int SRCOFFS_IIPLUS  = (SRCOFFS_40COL  +  256); //  256
const int SRCOFFS_80COL   = (SRCOFFS_IIPLUS +  256); //  512
const int SRCOFFS_LORES   = (SRCOFFS_80COL  +  128); //  640
const int SRCOFFS_HIRES   = (SRCOFFS_LORES  +   16); //  656
const int SRCOFFS_DHIRES  = (SRCOFFS_HIRES  +  512); // 1168
const int SRCOFFS_TOTAL   = (SRCOFFS_DHIRES + 2560); // 3278

const int MAX_SOURCE_Y = 512;
static LPBYTE        g_aSourceStartofLine[ MAX_SOURCE_Y ];
#define  SETSOURCEPIXEL(x,y,c)  g_aSourceStartofLine[(y)][(x)] = (c)


enum Color_Palette_Index_e
{
	// ...
// CUSTOM HGR COLORS (don't change order) - For tv emulation HGR Video Mode
	  HGR_BLACK        
	, HGR_WHITE        
	, HGR_BLUE         // HCOLOR=6 BLUE   , 3000: 81 00 D5 AA
	, HGR_ORANGE       // HCOLOR=5 ORANGE , 2C00: 82 00 AA D5
	, HGR_GREEN        // HCOLOR=1 GREEN  , 2400: 02 00 2A 55
	, HGR_VIOLET       // HCOLOR=2 VIOLET , 2800: 01 00 55 2A
	, HGR_GREY1        
	, HGR_GREY2        
	, HGR_YELLOW       
	, HGR_AQUA         
	, HGR_PURPLE       
	, HGR_PINK         
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

#define  SETRGBCOLOR(r,g,b) {b,g,r,0}

static RGBQUAD PalIndex2RGB[] =
{
	SETRGBCOLOR(/*HGR_BLACK, */ 0x00,0x00,0x00), // For TV emulation HGR Video Mode
	SETRGBCOLOR(/*HGR_WHITE, */ 0xFF,0xFF,0xFF),

//	SETRGBCOLOR(/*HGR_BLUE,  */   24, 115, 229), // HCOLOR=6 BLUE    3000: 81 00 D5 AA // 0x00,0x80,0xFF -> Linards Tweaked 0x0D,0xA1,0xFF
//	SETRGBCOLOR(/*HGR_ORANGE,*/  247,  64,  30), // HCOLOR=5 ORANGE  2C00: 82 00 AA D5 // 0xF0,0x50,0x00 -> Linards Tweaked 0xF2,0x5E,0x00 
//	SETRGBCOLOR(/*HGR_GREEN, */   27, 211,  79), // HCOLOR=1 GREEN   2400: 02 00 2A 55 // 0x20,0xC0,0x00 -> Linards Tweaked 0x38,0xCB,0x00
//	SETRGBCOLOR(/*HGR_VIOLET,*/  227,  20, 255), // HCOLOR=2 VIOLET  2800: 01 00 55 2A // 0xA0,0x00,0xFF -> Linards Tweaked 0xC7,0x34,0xFF
	SETRGBCOLOR(/*BLUE,      */ 0x0D,0xA1,0xFF), // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
	SETRGBCOLOR(/*ORANGE,    */ 0xF2,0x5E,0x00), // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
	SETRGBCOLOR(/*GREEN,     */ 0x38,0xCB,0x00), // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
	SETRGBCOLOR(/*MAGENTA,   */ 0xC7,0x34,0xFF), // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF


// For TV Emu:
//	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
//	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
//	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00), // 0xD0,0xB0,0x10 -> 0x9E,0x9E,0x00
//	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A), // 0x20,0xB0,0xB0 -> 0x00,0xCD,0x4A
//	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF), // 0x60,0x50,0xE0 -> 0x61,0x61,0xFF
//	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5), // 0xD0,0x40,0xA0 -> 0xFF,0x32,0xB5
};


enum VideoTypeOld_e		// AppleWin 1.25.0.4
{
	  VT_MONO_HALFPIXEL_REAL // uses custom monochrome
	, VT_COLOR_STANDARD
	, VT_COLOR_TEXT_OPTIMIZED 
	, VT_COLOR_TVEMU          
//	, VT_MONO_AMBER // now half pixel
//	, VT_MONO_GREEN // now half pixel
//	, VT_MONO_WHITE // now half pixel
//	, NUM_VIDEO_MODES
};

//static DWORD g_eVideoTypeOld = VT_COLOR_TVEMU;
static DWORD g_eVideoTypeOld = VT_COLOR_STANDARD;


static void V_CreateLookup_Hires()
{
//	int iMonochrome = GetMonochromeIndex();

	// BYTE colorval[6] = {MAGENTA,BLUE,GREEN,ORANGE,BLACK,WHITE};
	// BYTE colorval[6] = {HGR_MAGENTA,HGR_BLUE,HGR_GREEN,HGR_RED,HGR_BLACK,HGR_WHITE};
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
						if ((g_eVideoTypeOld == VT_COLOR_TVEMU) || !(aPixels[iPixel-2] && aPixels[iPixel+2]) )
							color = ((odd ^ !(iPixel&1)) << 1) | hibit;	// No white HGR text optimization
					}

					//if (g_eVideoTypeOld == VT_MONO_AUTHENTIC) {
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
						SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj  ,y  ,HiresToPalIndex[color]); // cTL
						SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj+1,y  ,HiresToPalIndex[color]); // cTR
						SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj  ,y+1,HiresToPalIndex[color]); // cBL
						SETSOURCEPIXEL(SRCOFFS_HIRES+coloffs+x+adj+1,y+1,HiresToPalIndex[color]); // cBR
					}
					x += 2;
				}
			}
		}
	}
}

#define HALF_PIXEL_SOLID 1
#define HALF_PIXEL_BLEED 0

void V_CreateLookup_HiResHalfPixel_Authentic() // Colors are solid (100% coverage)
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
*/

			// Fixup missing pixels that normally have been scan-line shifted -- Apple "half-pixel" -- but cross 14-pixel boundaries.
			if( hibit )
			{
				if ( aPixels[1] ) // preceeding pixel on?
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
						if ((g_eVideoTypeOld == VT_COLOR_STANDARD) || ( !aPixels[3] ))
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
							(g_eVideoTypeOld == VT_COLOR_STANDARD) // Fill in colors in between white pixels
						||	(g_eVideoTypeOld == VT_COLOR_TVEMU)    // Fill in colors in between white pixels (Post Processing will mix/merge colors)
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


static void CopySource(int /*dx*/, int /*dy*/, int w, int h, int sx, int sy, bgra_t *pVideoAddress)
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
			if (g_uHalfScanLines && !(h & 1))
			{
				// 50% Half Scan Line clears every odd scanline (and SHIFT+PrintScreen saves only the even rows)
				*(pDst+nBytes) = 0;
			}
			else
			{
				_ASSERT( *(pSrc+nBytes) < (sizeof(PalIndex2RGB)/sizeof(PalIndex2RGB[0])) );
				const RGBQUAD& rRGB = PalIndex2RGB[ *(pSrc+nBytes) ];
				const UINT32 rgb = (((UINT32)rRGB.rgbRed)<<16) | (((UINT32)rRGB.rgbGreen)<<8) | ((UINT32)rRGB.rgbBlue);
				*(pDst+nBytes) = rgb;
			}
		}

		pDst -= GetFrameBufferWidth();
		pSrc -= SRCOFFS_TOTAL;
	}
}

void UpdateHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress)
{
	int xpixel = x*14;
	int ypixel = y*16;

//	int offset = ((y & 7) << 7) + ((y >> 3) * 40) + x;
//	BYTE* g_pHiresBank0 = MemGetMainPtr(0x2000);
//	int  yoffset = 0;

	uint8_t *pMain = MemGetMainPtr(addr);
	BYTE byteval1 = (x >  0) ? *(pMain-1) : 0;
	BYTE byteval2 =            *(pMain);
	BYTE byteval3 = (x < 39) ? *(pMain+1) : 0;
	{
#define COLOFFS  (((byteval1 & 0x60) << 2) | ((byteval3 & 0x03) << 5))
#if 0
		if (g_eVideoTypeOld == VT_COLOR_TVEMU)
		{
			CopyMixedSource(
				xpixel >> 1, (ypixel+(yoffset >> 9)) >> 1,
				SRCOFFS_HIRES+COLOFFS+((x & 1) << 4), (((int)byteval2) << 1)
			);
		}
		else
#endif
		{
			CopySource(
				0,0, /*xpixel,ypixel*/ /*+(yoffset >> 9)*/
				14,2, // 2x upscale: 280x192 -> 560x384
				SRCOFFS_HIRES+COLOFFS+((x & 1) << 4), (((int)byteval2) << 1),
				pVideoAddress
			);
		}
#undef COLOFFS
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

	// First monochrome mode is seperate from others
//	if ((g_eVideoTypeOld >= VT_COLOR_STANDARD)	&& (g_eVideoTypeOld <  VT_MONO_AMBER))
	{
//		V_CreateLookup_Text(sourcedc);
//		V_CreateLookup_Lores();
		if ( g_eVideoType == VT_COLOR_TVEMU )
			V_CreateLookup_Hires();
		else
			V_CreateLookup_HiResHalfPixel_Authentic();
//		V_CreateLookup_DoubleHires();
	}
}

void VideoInitializeOriginal(void)
{
	// CREATE THE SOURCE IMAGE AND DRAW INTO THE SOURCE BIT BUFFER
	V_CreateDIBSections();
}
