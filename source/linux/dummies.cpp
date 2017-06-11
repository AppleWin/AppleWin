#include "StdAfx.h"

#include <iostream>

#include "SerialComms.h"
#include "MouseInterface.h"
#include "Configuration/IPropertySheet.h"
#include "Video.h"
#include "CPU.h"
#include "Memory.h"
#include "Keyboard.h"
#include "NTSC.h"
#include "Log.h"

static bool bVideoScannerNTSC = true;  // NTSC video scanning (or PAL)

// video scanner constants
int const kHBurstClock      =    53; // clock when Color Burst starts
int const kHBurstClocks     =     4; // clocks per Color Burst duration
int const kHClock0State     =  0x18; // H[543210] = 011000
int const kHClocks          =    65; // clocks per horizontal scan (including HBL)
int const kHPEClock         =    40; // clock when HPE (horizontal preset enable) goes low
int const kHPresetClock     =    41; // clock when H state presets
int const kHSyncClock       =    49; // clock when HSync starts
int const kHSyncClocks      =     4; // clocks per HSync duration
int const kNTSCScanLines    =   262; // total scan lines including VBL (NTSC)
int const kNTSCVSyncLine    =   224; // line when VSync starts (NTSC)
int const kPALScanLines     =   312; // total scan lines including VBL (PAL)
int const kPALVSyncLine     =   264; // line when VSync starts (PAL)
int const kVLine0State      = 0x100; // V[543210CBA] = 100000000
int const kVPresetLine      =   256; // line when V state presets
int const kVSyncLines       =     4; // lines per VSync duration
int const kVDisplayableScanLines = 192; // max displayable scanlines

eApple2Type GetApple2Type(void)
{
	return g_Apple2Type;
}

void SetApple2Type(eApple2Type type)
{
	g_Apple2Type = type;
	SetMainCpuDefault(type);
}

void DeleteCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void InitializeCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void EnterCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void LeaveCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void OutputDebugString(const char * str)
{
  std::cerr << str << std::endl;
}

UINT IPropertySheet::GetTheFreezesF8Rom(void)
{
  return 0;
}

int MessageBox(HWND, const char * text, const char * caption, UINT)
{
  LogFileOutput("MessageBox:\n%s\n%s\n\n", caption, text);
  return IDOK;
}

HWND GetDesktopWindow()
{
  return NULL;
}

void ExitProcess(int status)
{
  exit(status);
}

void alarm_log_too_many_alarms()
{
}

// Serial

CSuperSerialCard::CSuperSerialCard()
{
}

CSuperSerialCard::~CSuperSerialCard()
{
}

void CSuperSerialCard::CommInitialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
}

// Mouse

CMouseInterface::CMouseInterface()
{
}

CMouseInterface::~CMouseInterface()
{
}

void CMouseInterface::Initialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
}

void CMouseInterface::SetVBlank(bool bVBL)
{
}

// Joystick

void JoyportControl(const UINT uControl)
{
}

// Mockingboard

void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5)
{
}

void    MB_UpdateCycles(ULONG uExecutedCycles)
{
}

void    MB_StartOfCpuExecute()
{
}

// Video

int           g_nAltCharSetOffset  = 0; // alternate character set

#define  SW_80COL         (g_uVideoMode & VF_80COL)
#define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
#define  SW_HIRES         (g_uVideoMode & VF_HIRES)
#define  SW_80STORE       (g_uVideoMode & VF_80STORE)
#define  SW_MIXED         (g_uVideoMode & VF_MIXED)
#define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
#define  SW_TEXT          (g_uVideoMode & VF_TEXT)

bool    VideoGetSW80COL()
{
  return SW_80COL ? true : false;
}

bool    VideoGetVblBar(DWORD uExecutedCycles)
{
  // get video scanner position
  int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

  // calculate video parameters according to display standard
  const int kScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
  const int kScanCycles = kScanLines * kHClocks;
  nCycles %= kScanCycles;

  // VBL'
  return nCycles < kVDisplayableScanLines * kHClocks;
}

BYTE VideoCheckMode (WORD pc, WORD address, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
  address &= 0xFF;
  if (address == 0x7F)
  {
    return MemReadFloatingBus(SW_DHIRES != 0, uExecutedCycles);
  }
  else
  {
    BOOL result = 0;
    switch (address) {
    case 0x1A: result = SW_TEXT;    break;
    case 0x1B: result = SW_MIXED;   break;
    case 0x1D: result = SW_HIRES;   break;
    case 0x1E: result = g_nAltCharSetOffset;   break;
    case 0x1F: result = SW_80COL;   break;
    case 0x7F: result = SW_DHIRES;  break;
    }
    return KeybGetKeycode() | (result ? 0x80 : 0);
  }
}

BYTE VideoCheckVbl ( ULONG uExecutedCycles )
{
  bool bVblBar = VideoGetVblBar(uExecutedCycles);
  BYTE r = KeybGetKeycode();
  return (r & ~0x80) | (bVblBar ? 0x80 : 0);
}

BYTE VideoSetMode (WORD pc, WORD address, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
  address &= 0xFF;

  switch (address)
  {
  case 0x00:                 g_uVideoMode &= ~VF_80STORE;                            break;
  case 0x01:                 g_uVideoMode |=  VF_80STORE;                            break;
  case 0x0C: if (!IS_APPLE2){g_uVideoMode &= ~VF_80COL; NTSC_SetVideoTextMode(40);}; break;
  case 0x0D: if (!IS_APPLE2){g_uVideoMode |=  VF_80COL; NTSC_SetVideoTextMode(80);}; break;
  case 0x0E: if (!IS_APPLE2) g_nAltCharSetOffset = 0;           break;	// Alternate char set off
  case 0x0F: if (!IS_APPLE2) g_nAltCharSetOffset = 256;         break;	// Alternate char set on
  case 0x50: g_uVideoMode &= ~VF_TEXT;    break;
  case 0x51: g_uVideoMode |=  VF_TEXT;    break;
  case 0x52: g_uVideoMode &= ~VF_MIXED;   break;
  case 0x53: g_uVideoMode |=  VF_MIXED;   break;
  case 0x54: g_uVideoMode &= ~VF_PAGE2;   break;
  case 0x55: g_uVideoMode |=  VF_PAGE2;   break;
  case 0x56: g_uVideoMode &= ~VF_HIRES;   break;
  case 0x57: g_uVideoMode |=  VF_HIRES;   break;
  case 0x5E: if (!IS_APPLE2) g_uVideoMode |=  VF_DHIRES;  break;
  case 0x5F: if (!IS_APPLE2) g_uVideoMode &= ~VF_DHIRES;  break;
  }

  // Apple IIe, Technical Notes, #3: Double High-Resolution Graphics
  // 80STORE must be OFF to display page 2
  if (SW_80STORE)
    g_uVideoMode &= ~VF_PAGE2;

  // NTSC_BEGIN
  NTSC_SetVideoMode( g_uVideoMode );
  // NTSC_END

  return MemReadFloatingBus(uExecutedCycles);
}

void Video_ResetScreenshotCounter( char *pImageName )
{
}

//===========================================================================
//
// References to Jim Sather's books are given as eg:
// UTAIIe:5-7,P3 (Understanding the Apple IIe, chapter 5, page 7, Paragraph 3)
//
WORD VideoGetScannerAddress(bool* pbVblBar_OUT, const DWORD uExecutedCycles)
{
  // get video scanner position
  //
  int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

  // machine state switches
  //
  int nHires   = (SW_HIRES && !SW_TEXT) ? 1 : 0;
  int nPage2   = SW_PAGE2 ? 1 : 0;
  int n80Store = SW_80STORE ? 1 : 0;

  // calculate video parameters according to display standard
  //
  int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
  int nVSyncLine  = bVideoScannerNTSC ? kNTSCVSyncLine : kPALVSyncLine;
  int nScanCycles = nScanLines * kHClocks;
  nCycles %= nScanCycles;

  // calculate horizontal scanning state
  //
  int nHClock = (nCycles + kHPEClock) % kHClocks; // which horizontal scanning clock
  int nHState = kHClock0State + nHClock; // H state bits
  if (nHClock >= kHPresetClock) // check for horizontal preset
  {
    nHState -= 1; // correct for state preset (two 0 states)
  }
  int h_0 = (nHState >> 0) & 1; // get horizontal state bits
  int h_1 = (nHState >> 1) & 1;
  int h_2 = (nHState >> 2) & 1;
  int h_3 = (nHState >> 3) & 1;
  int h_4 = (nHState >> 4) & 1;
  int h_5 = (nHState >> 5) & 1;

  // calculate vertical scanning state
  //
  int nVLine  = nCycles / kHClocks; // which vertical scanning line
  int nVState = kVLine0State + nVLine; // V state bits
  if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
  {
    nVState -= nScanLines; // compensate for preset
  }
  int v_A = (nVState >> 0) & 1; // get vertical state bits
  int v_B = (nVState >> 1) & 1;
  int v_C = (nVState >> 2) & 1;
  int v_0 = (nVState >> 3) & 1;
  int v_1 = (nVState >> 4) & 1;
  int v_2 = (nVState >> 5) & 1;
  int v_3 = (nVState >> 6) & 1;
  int v_4 = (nVState >> 7) & 1;
  int v_5 = (nVState >> 8) & 1;

  // calculate scanning memory address
  //
  if (nHires && SW_MIXED && v_4 && v_2) // HIRES TIME signal (UTAIIe:5-7,P3)
  {
    nHires = 0; // address is in text memory for mixed hires
  }

  int nAddend0 = 0x0D; // 1            1            0            1
  int nAddend1 =              (h_5 << 2) | (h_4 << 1) | (h_3 << 0);
  int nAddend2 = (v_4 << 3) | (v_3 << 2) | (v_4 << 1) | (v_3 << 0);
  int nSum     = (nAddend0 + nAddend1 + nAddend2) & 0x0F; // SUM (UTAIIe:5-9)

  int nAddress = 0; // build address from video scanner equations (UTAIIe:5-8,T5.1)
  nAddress |= h_0  << 0; // a0
  nAddress |= h_1  << 1; // a1
  nAddress |= h_2  << 2; // a2
  nAddress |= nSum << 3; // a3 - a6
  nAddress |= v_0  << 7; // a7
  nAddress |= v_1  << 8; // a8
  nAddress |= v_2  << 9; // a9

  int p2a = !(nPage2 && !n80Store);
  int p2b = nPage2 && !n80Store;

  if (nHires) // hires?
  {
    // Y: insert hires-only address bits
    //
    nAddress |= v_A << 10; // a10
    nAddress |= v_B << 11; // a11
    nAddress |= v_C << 12; // a12
    nAddress |= p2a << 13; // a13
    nAddress |= p2b << 14; // a14
  }
  else
  {
    // N: insert text-only address bits
    //
    nAddress |= p2a << 10; // a10
    nAddress |= p2b << 11; // a11

    // Apple ][ (not //e) and HBL?
    //
    if (IS_APPLE2 && // Apple II only (UTAIIe:I-4,#5)
	!h_5 && (!h_4 || !h_3)) // HBL (UTAIIe:8-10,F8.5)
    {
      nAddress |= 1 << 12; // Y: a12 (add $1000 to address!)
    }
  }

  // update VBL' state
  //
  if (pbVblBar_OUT != NULL)
  {
    *pbVblBar_OUT = !v_4 || !v_3; // VBL' = (v_4 & v_3)' (UTAIIe:5-10,#3)
  }
  return static_cast<WORD>(nAddress);
}

// NTSC

void NTSC_VideoUpdateCycles( long cyclesLeftToUpdate )
{
}

/*
// Not used now
uint16_t NTSC_VideoGetScannerAddress()
{
  return 0;
}
*/

void NTSC_SetVideoMode( uint32_t uVideoModeFlags )
{
}

void NTSC_SetVideoTextMode( int cols )
{
}
