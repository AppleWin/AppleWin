#include "StdAfx.h"

#include "Video.h"
#include "NTSC.h"

#define VIDEO_SCANNER_MAX_HORZ   65 // TODO: use Video.cpp: kHClocks
#define VIDEO_SCANNER_MAX_VERT  262 // TODO: use Video.cpp: kNTSCScanLines
static const UINT VIDEO_SCANNER_6502_CYCLES = VIDEO_SCANNER_MAX_HORZ * VIDEO_SCANNER_MAX_VERT;
static UINT g_videoScanner6502Cycles = VIDEO_SCANNER_6502_CYCLES;	// default to NTSC

void NTSC_VideoUpdateCycles( UINT cycles6502 )
{
}

uint16_t NTSC_VideoGetScannerAddress( const ULONG uExecutedCycles )
{
  return VideoGetScannerAddress(uExecutedCycles);
}

void NTSC_SetVideoMode( uint32_t uVideoModeFlags, bool bDelay )
{
}

void NTSC_SetVideoTextMode( int cols )
{
}

UINT NTSC_GetCyclesPerFrame(void)
{
	return g_videoScanner6502Cycles;
}
