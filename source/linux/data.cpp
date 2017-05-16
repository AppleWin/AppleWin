#include "StdAfx.h"
#include "Common.h"

#include "Memory.h"
#include "SerialComms.h"
#include "MouseInterface.h"
#include "Speaker.h"
#include "Video.h"
#include "Configuration/IPropertySheet.h"
#include "SaveState_Structs_v1.h"


double		g_fCurrentCLK6502 = CLK_6502;	// Affected by Config dialog's speed slider bar
eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;
int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()
DWORD       g_dwCyclesThisFrame = 0;
bool      g_bFullSpeed      = false;
SS_CARDTYPE	g_Slot4 = CT_Empty;
SS_CARDTYPE	g_Slot5 = CT_Empty;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
TCHAR     g_sProgramDir[MAX_PATH] = TEXT(""); // Directory of where AppleWin executable resides
const TCHAR *g_pAppTitle = TITLE_APPLE_2E_ENHANCED;
bool      g_bRestart = false;
FILE*		g_fh			= NULL;
CSuperSerialCard	sg_SSC;
CMouseInterface		sg_Mouse;
const short		SPKR_DATA_INIT = (short)0x8000;

short		g_nSpeakerData	= SPKR_DATA_INIT;
bool			g_bQuieterSpeaker = false;
SoundType_e		soundtype		= SOUND_WAVE;

uint32_t  g_uVideoMode     = VF_TEXT; // Current Video Mode (this is the last set one as it may change mid-scan line!)

HINSTANCE g_hInstance = NULL;
HWND g_hFrameWindow = NULL;
static IPropertySheet * sg = NULL;
IPropertySheet&	sg_PropertySheet = *sg;
