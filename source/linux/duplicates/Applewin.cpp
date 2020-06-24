#include "StdAfx.h"
#include "Common.h"

#include "Memory.h"
#include "SerialComms.h"
#include "CardManager.h"
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

#include <unistd.h>

static const UINT VERSIONSTRING_SIZE = 16;
TCHAR VERSIONSTRING[VERSIONSTRING_SIZE] = "xx.yy.zz.ww";

HANDLE		g_hCustomRom = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $C000..$FFFF(16KiB) or $D000..$FFFF(12KiB)

static bool bLogKeyReadDone = false;
static DWORD dwLogKeyReadTickStart;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502_NTSC;	// Affected by Config dialog's speed slider bar
eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;
int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()
DWORD       g_dwCyclesThisFrame = 0;

// this is workaround to force Video to update h&v clock
// which should be done by NTSC_VideoUpdateCycles
// but it is not at the moment
bool      g_bFullSpeed      = true;

AppMode_e	g_nAppMode = MODE_LOGO;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
std::string     g_sProgramDir; // Directory of where AppleWin executable resides
std::string     g_sCurrentDir; // Also Starting Dir.  Debugger uses this when load/save
std::string     g_pAppTitle = TITLE_APPLE_2E_ENHANCED;
bool      g_bRestart = false;
CardManager g_CardMgr;
const short		SPKR_DATA_INIT = (short)0x8000;

short		g_nSpeakerData	= SPKR_DATA_INIT;
bool			g_bQuieterSpeaker = false;
SoundType_e		soundtype		= SOUND_WAVE;

HINSTANCE g_hInstance = NULL;
HWND g_hFrameWindow = NULL;
static IPropertySheet * sg = NULL;
IPropertySheet&	sg_PropertySheet = *sg;
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

void LogFileTimeUntilFirstKeyReadReset(void)
{
	if (!g_fh)
		return;

	dwLogKeyReadTickStart = GetTickCount();

	bLogKeyReadDone = false;
}

// Log the time from emulation restart/reboot until the first key read: BIT $C000
// . AZTEC.DSK (DOS 3.3) does prior LDY $C000 reads, but the BIT $C000 is at the "Press any key" message
// . Phasor1.dsk / ProDOS 1.1.1: PC=E797: B1 50: LDA ($50),Y / "Select an Option:" message
// . Rescue Raiders v1.3,v1.5: PC=895: LDA $C000 / boot to intro
void LogFileTimeUntilFirstKeyRead(void)
{
	if (!g_fh || bLogKeyReadDone)
		return;

	if ( (mem[regs.pc-3] != 0x2C)	// AZTEC: bit $c000
		&& !((regs.pc-2) == 0xE797 && mem[regs.pc-2] == 0xB1 && mem[regs.pc-1] == 0x50)	// Phasor1: lda ($50),y
		&& !((regs.pc-3) == 0x0895 && mem[regs.pc-3] == 0xAD)	// Rescue Raiders v1.3,v1.5: lda $c000
		)
		return;

	DWORD dwTime = GetTickCount() - dwLogKeyReadTickStart;

	LogFileOutput("Time from emulation reboot until first $C000 access: %d msec\n", dwTime);

	bLogKeyReadDone = true;
}

void SetLoadedSaveStateFlag(bool)
{
}

eApple2Type GetApple2Type(void)
{
	return g_Apple2Type;
}

void SetApple2Type(eApple2Type type)
{
	g_Apple2Type = type;
	SetMainCpuDefault(type);
}

bool SetCurrentImageDir(const std::string & dir ) {
  if (chdir(dir.c_str()) == 0)
    return true;
  else
    return false;
}

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

  g_fCurrentCLK6502 = CLK_6502_NTSC * g_fMHz;
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
    if (REGLOAD(TEXT(REGVALUE_OLD_APPLE2_TYPE), &dwComputerType))
    {
      switch (dwComputerType)
      {
	// NB. No A2TYPE_APPLE2E (this is correct)
      case 0:		apple2Type = A2TYPE_APPLE2; break;
      case 1:		apple2Type = A2TYPE_APPLE2PLUS; break;
      case 2:		apple2Type = A2TYPE_APPLE2EENHANCED; break;
      default:	apple2Type = A2TYPE_APPLE2EENHANCED;
      }
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
  if (REGLOAD(TEXT("Sound Emulation"), &dwSoundType))
  {
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

  DWORD dwEnhanceDisk;
  REGLOAD_DEFAULT(TEXT(REGVALUE_ENHANCE_DISK_SPEED), &dwEnhanceDisk, 1);
  g_CardMgr.GetDisk2CardMgr().SetEnhanceDisk(dwEnhanceDisk ? true : false);

  Config_Load_Video();
#if 0
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
    g_CardMgr.Insert(4, (SS_CARDTYPE)dwTmp);
  if(REGLOAD(TEXT(REGVALUE_SLOT5), &dwTmp))
    g_CardMgr.Insert(5, (SS_CARDTYPE)dwTmp);

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

  g_CardMgr.GetDisk2CardMgr().LoadLastDiskImage();

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
