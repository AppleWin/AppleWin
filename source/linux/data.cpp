#include "StdAfx.h"
#include "Common.h"

#include "Memory.h"
#include "SerialComms.h"
#include "MouseInterface.h"
#include "Speaker.h"
#include "Video.h"
#include "Configuration/IPropertySheet.h"
#include "SaveState_Structs_v1.h"
#include "Registry.h"
#include "Log.h"
#include "Applewin.h"
#include "CPU.h"
#include "Joystick.h"
#include "DiskImage.h"
#include "Disk.h"
#include "Harddisk.h"
#include "ParallelPrinter.h"
#include "SaveState.h"

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502;	// Affected by Config dialog's speed slider bar
eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;
int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()
DWORD       g_dwCyclesThisFrame = 0;
bool      g_bFullSpeed      = false;
SS_CARDTYPE	g_Slot4 = CT_Empty;
SS_CARDTYPE	g_Slot5 = CT_Empty;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
TCHAR     g_sProgramDir[MAX_PATH] = TEXT(""); // Directory of where AppleWin executable resides
TCHAR     g_sCurrentDir[MAX_PATH] = TEXT(""); // Also Starting Dir.  Debugger uses this when load/save
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
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

void SetCurrentCLK6502(void)
{
  static DWORD dwPrevSpeed = (DWORD) -1;

  if(dwPrevSpeed == g_dwSpeed)
    return;

  dwPrevSpeed = g_dwSpeed;

  // SPEED_MIN    =  0 = 0.50 MHz
  // SPEED_NORMAL = 10 = 1.00 MHz
  //                20 = 2.00 MHz
  // SPEED_MAX-1  = 39 = 3.90 MHz
  // SPEED_MAX    = 40 = ???? MHz (run full-speed, /g_fCurrentCLK6502/ is ignored)

  if(g_dwSpeed < SPEED_NORMAL)
    g_fMHz = 0.5 + (double)g_dwSpeed * 0.05;
  else
    g_fMHz = (double)g_dwSpeed / 10.0;

  g_fCurrentCLK6502 = CLK_6502 * g_fMHz;
}

void SetWindowTitle()
{
  switch (g_Apple2Type)
  {
  default:
  case A2TYPE_APPLE2:			g_pAppTitle = TITLE_APPLE_2; break;
  case A2TYPE_APPLE2PLUS:		g_pAppTitle = TITLE_APPLE_2_PLUS; break;
  case A2TYPE_APPLE2E:                  g_pAppTitle = TITLE_APPLE_2E; break;
  case A2TYPE_APPLE2EENHANCED:          g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break;
  case A2TYPE_PRAVETS82:		g_pAppTitle = TITLE_PRAVETS_82; break;
  case A2TYPE_PRAVETS8M:		g_pAppTitle = TITLE_PRAVETS_8M; break;
  case A2TYPE_PRAVETS8A:		g_pAppTitle = TITLE_PRAVETS_8A; break;
  case A2TYPE_TK30002E:                 g_pAppTitle = TITLE_TK3000_2E; break;
  }
}

void LoadConfiguration(void)
{
  DWORD dwComputerType;
  eApple2Type apple2Type = A2TYPE_APPLE2EENHANCED;

  if (REGLOAD(TEXT(REGVALUE_APPLE2_TYPE), &dwComputerType))
  {
    const DWORD dwLoadedComputerType = dwComputerType;

    if ( (dwComputerType >= A2TYPE_MAX) ||
	 (dwComputerType >= A2TYPE_UNDEFINED && dwComputerType < A2TYPE_CLONE) ||
	 (dwComputerType >= A2TYPE_CLONE_A2_MAX && dwComputerType < A2TYPE_CLONE_A2E) )
      dwComputerType = A2TYPE_APPLE2EENHANCED;

    // Remap the bad Pravets models (before AppleWin v1.26)
    if (dwComputerType == A2TYPE_BAD_PRAVETS82) dwComputerType = A2TYPE_PRAVETS82;
    if (dwComputerType == A2TYPE_BAD_PRAVETS8M) dwComputerType = A2TYPE_PRAVETS8M;

    // Remap the bad Pravets models (at AppleWin v1.26) - GH#415
    if (dwComputerType == A2TYPE_CLONE) dwComputerType = A2TYPE_PRAVETS82;

    if (dwLoadedComputerType != dwComputerType)
    {
      char sText[ 100 ];
      _snprintf( sText, sizeof(sText)-1, "Unsupported Apple2Type(%d). Changing to %d", dwLoadedComputerType, dwComputerType);

      MessageBox(
		 GetDesktopWindow(),		// NB. g_hFrameWindow is not yet valid
		 sText,
		 "Load Configuration",
		 MB_ICONSTOP | MB_SETFOREGROUND);

      LogFileOutput("%s\n", sText);

      REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), dwComputerType);
    }

    apple2Type = (eApple2Type) dwComputerType;
  }
  else	// Support older AppleWin registry entries
  {
    REGLOAD(TEXT(REGVALUE_OLD_APPLE2_TYPE), &dwComputerType);
    switch (dwComputerType)
    {
      // NB. No A2TYPE_APPLE2E (this is correct)
    case 0:		apple2Type = A2TYPE_APPLE2; break;
    case 1:		apple2Type = A2TYPE_APPLE2PLUS; break;
    case 2:		apple2Type = A2TYPE_APPLE2EENHANCED; break;
    default:	apple2Type = A2TYPE_APPLE2EENHANCED;
    }
  }

  SetApple2Type(apple2Type);

  //

  DWORD dwCpuType;
  eCpuType cpu = CPU_65C02;

  if (REGLOAD(TEXT(REGVALUE_CPU_TYPE), &dwCpuType))
  {
    if (dwCpuType != CPU_6502 && dwCpuType != CPU_65C02)
      dwCpuType = CPU_65C02;

    cpu = (eCpuType) dwCpuType;
  }

  SetMainCpu(cpu);

  //
#if 0
  DWORD dwJoyType;
  if (REGLOAD(TEXT(REGVALUE_JOYSTICK0_EMU_TYPE), &dwJoyType))
    JoySetJoyType(JN_JOYSTICK0, dwJoyType);
  else
    LoadConfigOldJoystick(JN_JOYSTICK0);

  if (REGLOAD(TEXT(REGVALUE_JOYSTICK1_EMU_TYPE), &dwJoyType))
    JoySetJoyType(JN_JOYSTICK1, dwJoyType);
  else
    LoadConfigOldJoystick(JN_JOYSTICK1);

  DWORD dwSoundType;
  REGLOAD(TEXT("Sound Emulation"), &dwSoundType);
  switch (dwSoundType)
  {
  case REG_SOUNDTYPE_NONE:
  case REG_SOUNDTYPE_DIRECT:	// Not supported from 1.26
  case REG_SOUNDTYPE_SMART:	// Not supported from 1.26
  default:
    soundtype = SOUND_NONE;
    break;
  case REG_SOUNDTYPE_WAVE:
    soundtype = SOUND_WAVE;
    break;
  }

  char aySerialPortName[ CSuperSerialCard::SIZEOF_SERIALCHOICE_ITEM ];
  if (RegLoadString(	TEXT("Configuration"),
			TEXT(REGVALUE_SERIAL_PORT_NAME),
			TRUE,
			aySerialPortName,
			sizeof(aySerialPortName) ) )
  {
    sg_SSC.SetSerialPortName(aySerialPortName);
  }
#endif
  REGLOAD(TEXT(REGVALUE_EMULATION_SPEED)   ,&g_dwSpeed);

  REGLOAD(TEXT(REGVALUE_ENHANCE_DISK_SPEED), &enhancedisk);

#if 0
  Config_Load_Video();
  REGLOAD(TEXT("Uthernet Active"), &tfe_enabled);
#endif
  SetCurrentCLK6502();

  //

  DWORD dwTmp;

#if 0
  if(REGLOAD(TEXT(REGVALUE_THE_FREEZES_F8_ROM), &dwTmp))
    sg_PropertySheet.SetTheFreezesF8Rom(dwTmp);

  if(REGLOAD(TEXT(REGVALUE_SPKR_VOLUME), &dwTmp))
    SpkrSetVolume(dwTmp, sg_PropertySheet.GetVolumeMax());

  if(REGLOAD(TEXT(REGVALUE_MB_VOLUME), &dwTmp))
    MB_SetVolume(dwTmp, sg_PropertySheet.GetVolumeMax());

  if(REGLOAD(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), &dwTmp))
    g_bSaveStateOnExit = dwTmp ? true : false;


  if(REGLOAD(TEXT(REGVALUE_DUMP_TO_PRINTER), &dwTmp))
    g_bDumpToPrinter = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_CONVERT_ENCODING), &dwTmp))
    g_bConvertEncoding = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_FILTER_UNPRINTABLE), &dwTmp))
    g_bFilterUnprintable = dwTmp ? true : false;

  if(REGLOAD(TEXT(REGVALUE_PRINTER_APPEND), &dwTmp))
    g_bPrinterAppend = dwTmp ? true : false;
#endif

  if(REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
    HD_SetEnabled(dwTmp ? true : false);

#if 0
  if(REGLOAD(TEXT(REGVALUE_PDL_XTRIM), &dwTmp))
    JoySetTrim((short)dwTmp, true);
  if(REGLOAD(TEXT(REGVALUE_PDL_YTRIM), &dwTmp))
    JoySetTrim((short)dwTmp, false);

  if(REGLOAD(TEXT(REGVALUE_SCROLLLOCK_TOGGLE), &dwTmp))
    sg_PropertySheet.SetScrollLockToggle(dwTmp);

  if(REGLOAD(TEXT(REGVALUE_CURSOR_CONTROL), &dwTmp))
    sg_PropertySheet.SetJoystickCursorControl(dwTmp);
  if(REGLOAD(TEXT(REGVALUE_AUTOFIRE), &dwTmp))
    sg_PropertySheet.SetAutofire(dwTmp);
  if(REGLOAD(TEXT(REGVALUE_CENTERING_CONTROL), &dwTmp))
    sg_PropertySheet.SetJoystickCenteringControl(dwTmp);

  if(REGLOAD(TEXT(REGVALUE_MOUSE_CROSSHAIR), &dwTmp))
    sg_PropertySheet.SetMouseShowCrosshair(dwTmp);
  if(REGLOAD(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW), &dwTmp))
    sg_PropertySheet.SetMouseRestrictToWindow(dwTmp);
#endif

  if(REGLOAD(TEXT(REGVALUE_SLOT4), &dwTmp))
    g_Slot4 = (SS_CARDTYPE) dwTmp;
  if(REGLOAD(TEXT(REGVALUE_SLOT5), &dwTmp))
    g_Slot5 = (SS_CARDTYPE) dwTmp;

  //

  char szFilename[MAX_PATH] = {0};

  RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, szFilename, MAX_PATH);
  if (szFilename[0] == 0)
    GetCurrentDirectory(sizeof(szFilename), szFilename);
  SetCurrentImageDir(szFilename);

  HD_LoadLastDiskImage(HARDDISK_1);
  HD_LoadLastDiskImage(HARDDISK_2);

  //

  // Current/Starting Dir is the "root" of where the user keeps his disk images
  RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, szFilename, MAX_PATH);
  if (szFilename[0] == 0)
    GetCurrentDirectory(sizeof(szFilename), szFilename);
  SetCurrentImageDir(szFilename);

  Disk_LoadLastDiskImage(DRIVE_1);
  Disk_LoadLastDiskImage(DRIVE_2);

  //

  szFilename[0] = 0;
  RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_SAVESTATE_FILENAME),1,szFilename,sizeof(szFilename));
  Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used (ie. g_sCurrentDir + default filename)

  szFilename[0] = 0;
  RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_PRINTER_FILENAME),1,szFilename,sizeof(szFilename));
  Printer_SetFilename(szFilename);	// If not in Registry than default will be used

  dwTmp = 10;
  REGLOAD(TEXT(REGVALUE_PRINTER_IDLE_LIMIT), &dwTmp);
  Printer_SetIdleLimit(dwTmp);

#if 0
  char szUthernetInt[MAX_PATH] = {0};
  RegLoadString(TEXT(REG_CONFIG),TEXT("Uthernet Interface"),1,szUthernetInt,MAX_PATH);
  update_tfe_interface(szUthernetInt,NULL);

  if (REGLOAD(TEXT(REGVALUE_WINDOW_SCALE), &dwTmp))
    SetViewportScale(dwTmp);

  if (REGLOAD(TEXT(REGVALUE_CONFIRM_REBOOT), &dwTmp))
    g_bConfirmReboot = dwTmp;
#endif
}

void CheckCpu()
{
  const eApple2Type apple2Type = GetApple2Type();
  const eCpuType defaultCpu = ProbeMainCpuDefault(apple2Type);
  const eCpuType mainCpu = GetMainCpu();
  if (mainCpu != defaultCpu)
  {
    LogFileOutput("Detected non standard CPU for Apple2 = %d: default = %d, actual = %d\n", apple2Type, defaultCpu, mainCpu);
  }
}
