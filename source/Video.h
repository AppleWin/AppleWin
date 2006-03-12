#pragma once

enum VIDEOTYPE
{
	  VT_MONO_CUSTOM
	, VT_COLOR_STANDARD
	, VT_COLOR_TEXT_OPTIMIZED
	, VT_COLOR_TVEMU
	, VT_COLOR_HALF_SHIFT_DIM
	, VT_MONO_AMBER
	, VT_MONO_GREEN
	, VT_MONO_WHITE
	, VT_NUM_MODES
};

extern BOOL       graphicsmode;
extern COLORREF   monochrome;
extern DWORD      videotype;

void    CreateColorMixMap();

BOOL    VideoApparentlyDirty ();
void    VideoBenchmark ();
void    VideoCheckPage (BOOL);
void    VideoChooseColor ();
void    VideoDestroy ();
void    VideoDisplayLogo ();
BOOL    VideoHasRefreshed ();
void    VideoInitialize ();
void    VideoRealizePalette (HDC);
void    VideoRedrawScreen ();
void    VideoRefreshScreen ();
void    VideoReinitialize ();
void    VideoResetState ();
WORD    VideoGetScannerAddress(bool* pbVblBar_OUT = NULL);
void    VideoUpdateVbl (DWORD dwCyclesThisFrame);
void    VideoUpdateFlash();
bool    VideoGetSW80COL();
DWORD   VideoGetSnapshot(SS_IO_Video* pSS);
DWORD   VideoSetSnapshot(SS_IO_Video* pSS);

BYTE __stdcall VideoCheckMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoCheckVbl (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoSetMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
