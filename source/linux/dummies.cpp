#include "StdAfx.h"

#include <iostream>

#include "SerialComms.h"
#include "MouseInterface.h"
#include "Configuration/IPropertySheet.h"
#include "Video.h"
#include "Memory.h"
#include "Keyboard.h"
#include "NTSC.h"

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
  std::cerr << caption << " - " << text << std::endl;
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

// Registry

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser,
                    LPTSTR buffer, DWORD chars)
{
  std::cerr << "RegLoadString: " << section << " - " << key << std::endl;
  return FALSE;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value)
{
  std::cerr << "RegLoadValue: " << section << " - " << key << std::endl;
  return FALSE;
}

void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer)
{
  std::cerr << "RegSaveString: " << section << " - " << key << " = " << buffer << std::endl;
}

void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value)
{
  std::cerr << "RegSaveValue: " << section << " - " << key << " = " << value << std::endl;
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
  return 0;
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

// NTSC

void NTSC_VideoUpdateCycles( long cyclesLeftToUpdate )
{
}

uint16_t NTSC_VideoGetScannerAddress()
{
  return 0;
}

void NTSC_SetVideoMode( uint32_t uVideoModeFlags )
{
}

void NTSC_SetVideoTextMode( int cols )
{
}
