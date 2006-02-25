#pragma once

enum VIDEOTYPE {VT_MONO=0, VT_COLOR_STANDARD, VT_COLOR_TEXT_OPTIMIZED, VT_COLOR_TVEMU, VT_NUM_MODES};

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
void    VideoUpdateVbl (DWORD dwCyclesThisFrame);
void    VideoUpdateFlash();
bool    VideoGetSW80COL();
DWORD   VideoGetSnapshot(SS_IO_Video* pSS);
DWORD   VideoSetSnapshot(SS_IO_Video* pSS);

BYTE __stdcall VideoCheckMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoCheckVbl (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall VideoSetMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
