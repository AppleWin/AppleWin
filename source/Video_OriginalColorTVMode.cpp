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
	SETRGBCOLOR(/*HGR_BLUE,  */   24, 115, 229), // HCOLOR=6 BLUE    3000: 81 00 D5 AA // 0x00,0x80,0xFF -> Linards Tweaked 0x0D,0xA1,0xFF
	SETRGBCOLOR(/*HGR_ORANGE,*/  247,  64,  30), // HCOLOR=5 ORANGE  2C00: 82 00 AA D5 // 0xF0,0x50,0x00 -> Linards Tweaked 0xF2,0x5E,0x00 
	SETRGBCOLOR(/*HGR_GREEN, */   27, 211,  79), // HCOLOR=1 GREEN   2400: 02 00 2A 55 // 0x20,0xC0,0x00 -> Linards Tweaked 0x38,0xCB,0x00
	SETRGBCOLOR(/*HGR_VIOLET,*/  227,  20, 255), // HCOLOR=2 VIOLET  2800: 01 00 55 2A // 0xA0,0x00,0xFF -> Linards Tweaked 0xC7,0x34,0xFF

	SETRGBCOLOR(/*HGR_GREY1, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_GREY2, */ 0x80,0x80,0x80),
	SETRGBCOLOR(/*HGR_YELLOW,*/ 0x9E,0x9E,0x00), // 0xD0,0xB0,0x10 -> 0x9E,0x9E,0x00
	SETRGBCOLOR(/*HGR_AQUA,  */ 0x00,0xCD,0x4A), // 0x20,0xB0,0xB0 -> 0x00,0xCD,0x4A
	SETRGBCOLOR(/*HGR_PURPLE,*/ 0x61,0x61,0xFF), // 0x60,0x50,0xE0 -> 0x61,0x61,0xFF
	SETRGBCOLOR(/*HGR_PINK,  */ 0xFF,0x32,0xB5), // 0xD0,0x40,0xA0 -> 0xFF,0x32,0xB5
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

static DWORD g_eVideoTypeOld = VT_COLOR_TVEMU;


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

//const UINT FRAMEBUFFER_H = 192*2;
//static LPBYTE        g_aFrameBufferOffset[FRAMEBUFFER_H]; // array of pointers to start of each scanline
//static UINT g_nFrameBufferPitch = 0;	// TODO

static void CopySource(int /*dx*/, int /*dy*/, int w, int h, int sx, int sy, bgra_t *pVideoAddress)
{
//	UINT32* pDst = (UINT32*) (g_aFrameBufferOffset[ dy ] + dx*sizeof(UINT32));
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
//				const RGBQUAD& rRGB = GetFrameBufferInfo()->bmiColors[ *(pSrc+nBytes) ];
				_ASSERT( *(pSrc+nBytes) < (sizeof(PalIndex2RGB)/sizeof(PalIndex2RGB[0])) );
				const RGBQUAD& rRGB = PalIndex2RGB[ *(pSrc+nBytes) ];
				const UINT32 rgb = (((UINT32)rRGB.rgbRed)<<16) | (((UINT32)rRGB.rgbGreen)<<8) | ((UINT32)rRGB.rgbBlue);
				*(pDst+nBytes) = rgb;
			}
		}

//		pDst -= g_nFrameBufferPitch / sizeof(UINT32);
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

static HBITMAP       g_hSourceBitmap = NULL;
static LPBYTE        g_pSourcePixels = NULL;
static LPBITMAPINFO  g_pSourceHeader = NULL;

static void V_CreateDIBSections(void) 
{
//	CopyMemory(g_pSourceHeader->bmiColors,GetFrameBufferInfo()->bmiColors,256*sizeof(RGBQUAD));

	// CREATE THE DEVICE CONTEXT
	HWND window  = GetDesktopWindow();
	HDC dc       = GetDC(window);
#if 0
	if (g_hDeviceDC)
	{
		DeleteDC(g_hDeviceDC);
	}
	g_hDeviceDC = CreateCompatibleDC(dc);

	// CREATE THE FRAME BUFFER DIB SECTION
	if (g_hDeviceBitmap)
		DeleteObject(g_hDeviceBitmap);
		g_hDeviceBitmap = CreateDIBSection(
			dc,
			g_pFramebufferinfo,
			DIB_RGB_COLORS,
			(LPVOID *)&g_pFramebufferbits,0,0
		);
	SelectObject(g_hDeviceDC,g_hDeviceBitmap);
#endif

	// CREATE THE SOURCE IMAGE DIB SECTION
	HDC sourcedc = CreateCompatibleDC(dc);
	ReleaseDC(window,dc);
	if (g_hSourceBitmap)
		DeleteObject(g_hSourceBitmap);

	g_hSourceBitmap = CreateDIBSection(
		sourcedc,
		g_pSourceHeader,
		DIB_RGB_COLORS,
		(LPVOID *)&g_pSourcePixels,0,0
	);
	SelectObject(sourcedc,g_hSourceBitmap);

	// CREATE THE OFFSET TABLE FOR EACH SCAN LINE IN THE SOURCE IMAGE
	for (int y = 0; y < MAX_SOURCE_Y; y++)
		g_aSourceStartofLine[ y ] = g_pSourcePixels + SRCOFFS_TOTAL*((MAX_SOURCE_Y-1) - y);

	// DRAW THE SOURCE IMAGE INTO THE SOURCE BIT BUFFER
	ZeroMemory(g_pSourcePixels,SRCOFFS_TOTAL*512); // 32 bytes/pixel * 16 colors = 512 bytes/row

	// First monochrome mode is seperate from others
//	if ((g_eVideoTypeOld >= VT_COLOR_STANDARD)	&& (g_eVideoTypeOld <  VT_MONO_AMBER))
	{
//		V_CreateLookup_Text(sourcedc);
//		V_CreateLookup_Lores();
		V_CreateLookup_Hires();
//		V_CreateLookup_DoubleHires();
	}

	DeleteDC(sourcedc);
}

void VideoInitializeOriginal(void)
{
//	_ASSERT(GetFrameBufferInfo());

	// CREATE A BITMAPINFO STRUCTURE FOR THE SOURCE IMAGE
	g_pSourceHeader = (LPBITMAPINFO)VirtualAlloc(
		NULL,
		sizeof(BITMAPINFOHEADER) + 256*sizeof(RGBQUAD),
		MEM_COMMIT,
		PAGE_READWRITE);

	ZeroMemory(g_pSourceHeader,sizeof(BITMAPINFOHEADER));
	g_pSourceHeader->bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	g_pSourceHeader->bmiHeader.biWidth    = SRCOFFS_TOTAL;
	g_pSourceHeader->bmiHeader.biHeight   = 512;
	g_pSourceHeader->bmiHeader.biPlanes   = 1;
	g_pSourceHeader->bmiHeader.biBitCount = 8;
	g_pSourceHeader->bmiHeader.biClrUsed  = 256;

//	// VideoReinitialize() ... except we set the frame buffer palette....
//	V_CreateIdentityPalette();

#if 0
	//RGB() -> none
	//PALETTERGB() -> PC_EXPLICIT
	//??? RGB() -> PC_NOCOLLAPSE
	for( int iColor = 0; iColor < NUM_COLOR_PALETTE; iColor++ )
		customcolors[ iColor ] = ((DWORD)PC_EXPLICIT << 24) | RGB(
		GetFrameBufferInfo()->bmiColors[iColor].rgbRed,
		GetFrameBufferInfo()->bmiColors[iColor].rgbGreen,
		GetFrameBufferInfo()->bmiColors[iColor].rgbBlue
	);
#endif

	// CREATE THE FRAME BUFFER DIB SECTION AND DEVICE CONTEXT,
	// CREATE THE SOURCE IMAGE DIB SECTION AND DRAW INTO THE SOURCE BIT BUFFER
	V_CreateDIBSections();
}
