#pragma once

#define SUPPORT_CPM
#define USE_SPEECH_API

const double _M14 = (157500000.0 / 11.0); // 14.3181818... * 10^6
const double CLK_6502 = ((_M14 * 65.0) / 912.0); // 65 cycles per 912 14M clocks
//const double CLK_6502 = 23 * 44100;			// 1014300

// The effective Z-80 clock rate is 2.041MHz
// See: http://www.apple2info.net/hardware/softcard/SC-SWHW_a2in.pdf
const double CLK_Z80 = (CLK_6502 * 2);

const UINT uCyclesPerLine			= 65;	// 25 cycles of HBL & 40 cycles of HBL'
const UINT uVisibleLinesPerFrame	= 64*3;	// 192
const UINT uLinesPerFrame			= 262;	// 64 in each third of the screen & 70 in VBL
const DWORD dwClksPerFrame			= uCyclesPerLine * uLinesPerFrame;	// 17030

#define NUM_SLOTS 8

#define  MAX(a,b)          (((a) > (b)) ? (a) : (b))
#define  MIN(a,b)          (((a) < (b)) ? (a) : (b))

#define  RAMWORKS			// 8MB RamWorks III support

// Use a base freq so that DirectX (or sound h/w) doesn't have to up/down-sample
// Assume base freqs are 44.1KHz & 48KHz
const DWORD SPKR_SAMPLE_RATE = 44100;

enum AppMode_e
{
	MODE_LOGO = 0
	, MODE_PAUSED
	, MODE_RUNNING  // 6502 is running at normal speed (Debugger breakpoints may or may not be active)
	, MODE_DEBUG    // 6502 is paused
	, MODE_STEPPING // 6502 is running at full speed (Debugger breakpoints always active)
};

#define  SPEED_MIN         0
#define  SPEED_NORMAL      10
#define  SPEED_MAX         40

#define  DRAW_BACKGROUND   1
#define  DRAW_LEDS         2
#define  DRAW_TITLE        4
#define  DRAW_BUTTON_DRIVES 8

#define  BTN_HELP          0
#define  BTN_RUN           1
#define  BTN_DRIVE1        2
#define  BTN_DRIVE2        3
#define  BTN_DRIVESWAP     4
#define  BTN_FULLSCR       5
#define  BTN_DEBUG         6
#define  BTN_SETUP         7
#define  BTN_P8CAPS        9

// TODO: Move to StringTable.h
#define	TITLE_APPLE_2			TEXT("Apple ][ Emulator")
#define	TITLE_APPLE_2_PLUS		TEXT("Apple ][+ Emulator")
#define	TITLE_APPLE_2E			TEXT("Apple //e Emulator")
#define	TITLE_APPLE_2E_ENHANCED	TEXT("Enhanced Apple //e Emulator")
#define TITLE_APPLE_2C          TEXT("Apple //e Emulator")
#define TITLE_APPLE_2D          TEXT("Apple )(d Virtual Debug Hardware") 
#define	TITLE_PRAVETS_82        TEXT("Pravets 82 Emulator")
#define	TITLE_PRAVETS_8M        TEXT("Pravets 8M Emulator")
#define	TITLE_PRAVETS_8A        TEXT("Pravets 8A Emulator")

#define TITLE_PAUSED       TEXT("* PAUSED *")
#define TITLE_STEPPING     TEXT("Stepping")

#define  REGLOAD(a,b) RegLoadValue(TEXT("Configuration"),a,1,b)
#define  REGSAVE(a,b) RegSaveValue(TEXT("Configuration"),a,1,b)

// Configuration
#define REG_CONFIG						"Configuration"
#define  REGVALUE_APPLE2_TYPE        "Apple2 Type"
#define  REGVALUE_SPKR_VOLUME        "Speaker Volume"
#define  REGVALUE_MB_VOLUME          "Mockingboard Volume"
#define  REGVALUE_SOUNDCARD_TYPE     "Soundcard Type"
#define  REGVALUE_SAVESTATE_FILENAME "Save State Filename"
#define  REGVALUE_SAVE_STATE_ON_EXIT "Save State On Exit"
#define  REGVALUE_HDD_ENABLED        "Harddisk Enable"
#define  REGVALUE_PDL_XTRIM          "PDL X-Trim"
#define  REGVALUE_PDL_YTRIM          "PDL Y-Trim"
#define  REGVALUE_SCROLLLOCK_TOGGLE  "ScrollLock Toggle"
#define  REGVALUE_MOUSE_IN_SLOT4     "Mouse in slot 4"
#define  REGVALUE_MOUSE_CROSSHAIR    "Mouse crosshair"
#define  REGVALUE_MOUSE_RESTRICT_TO_WINDOW "Mouse restrict to window"
#define  REGVALUE_THE_FREEZES_F8_ROM "The Freeze's F8 Rom"
#define  REGVALUE_CLONETYPE          "Clone Type"
#define  REGVALUE_CIDERPRESSLOC      "CiderPress Location"
#define  REGVALUE_Z80_IN_SLOT5       "Z80 in slot 5"
#define  REGVALUE_DUMP_TO_PRINTER    "Dump to printer"
#define  REGVALUE_CONVERT_ENCODING   "Convert printer encoding for clones"
#define  REGVALUE_FILTER_UNPRINTABLE "Filter unprintable characters"
#define  REGVALUE_PRINTER_FILENAME   "Printer Filename"
#define  REGVALUE_PRINTER_APPEND     "Append to printer file"
#define  REGVALUE_PRINTER_IDLE_LIMIT "Printer idle limit"
#define  REGVALUE_VIDEO_MODE         "Video Emulation"
#define  REGVALUE_VIDEO_HALF_SCAN_LINES "Half Scan Lines"
#define  REGVALUE_VIDEO_MONO_COLOR      "Monochrome Color"
#define  REGVALUE_SERIAL_PORT_NAME   "Serial Port Name"
#define  REGVALUE_ENHANCE_DISK_SPEED "Enhance Disk Speed"

// Preferences 
#define REG_PREFS						"Preferences"
#define REGVALUE_PREF_START_DIR      "Starting Directory"
#define REGVALUE_PREF_LAST_DISK_1	 "Last Disk Image 1"
#define REGVALUE_PREF_LAST_DISK_2	 "Last Disk Image 2"
#define REGVALUE_PREF_WINDOW_X_POS   "Window X-Position"
#define REGVALUE_PREF_WINDOW_Y_POS   "Window Y-Position"
#define REGVALUE_PREF_HDV_START_DIR  "HDV Starting Directory"
#define REGVALUE_PREF_LAST_HARDDISK_1 "Last Harddisk Image 1"
#define REGVALUE_PREF_LAST_HARDDISK_2 "Last Harddisk Image 2"

#define WM_USER_BENCHMARK	WM_USER+1
#define WM_USER_RESTART		WM_USER+2
#define WM_USER_SAVESTATE	WM_USER+3
#define WM_USER_LOADSTATE	WM_USER+4
#define VK_SNAPSHOT_560		WM_USER+5
#define VK_SNAPSHOT_280		WM_USER+6
#define WM_USER_TCP_SERIAL	WM_USER+7
#define WM_USER_BOOT		WM_USER+8

enum eSOUNDCARDTYPE {SC_UNINIT=0, SC_NONE, SC_MOCKINGBOARD, SC_PHASOR};	// Apple soundcard type

typedef BYTE (__stdcall *iofunction)(WORD nPC, WORD nAddr, BYTE nWriteFlag, BYTE nWriteValue, ULONG nCyclesLeft);

typedef struct _IMAGE__ { int unused; } *HIMAGE;	// DiskImage's /ImageInfo/ is hidden behind HIMAGE

enum eIRQSRC {IS_6522=0, IS_SPEECH, IS_SSC, IS_MOUSE};

//
#define APPLE2P_MASK    0x01
/*
 ][    0
 ][+   1
 //e  10
 //e+ 11
 //c  20
 //d  40
*/
#define APPLE2E_MASK	0x10
#define APPLE2C_MASK	0x20
#define APPLE2D_MASK    0x40
#define APPLECLONE_MASK	0x100

#define IS_APPLE2		((g_Apple2Type & (APPLE2E_MASK|APPLE2C_MASK)) == 0)
#define IS_APPLE2E		(g_Apple2Type & APPLE2E_MASK)
#define IS_APPLE2C		(g_Apple2Type & APPLE2C_MASK)

// NB. These get persisted to the Registry, so don't change the values for these enums!
enum eApple2Type {
					A2TYPE_APPLE2=0,
					A2TYPE_APPLE2PLUS,
					A2TYPE_APPLE2E=APPLE2E_MASK,
					A2TYPE_APPLE2EEHANCED,
					A2TYPE_UNDEFINED,
					A2TYPE_APPLE2C=APPLE2C_MASK,
					A2TYPE_APPLE2D=APPLE2D_MASK,
					//
					// Clones start here:
					A2TYPE_CLONE=APPLECLONE_MASK,
					A2TYPE_PRAVETS82=APPLECLONE_MASK|APPLE2E_MASK,
					A2TYPE_PRAVETS8M,
					A2TYPE_PRAVETS8A,
					A2TYPE_MAX
				};

enum eBUTTON {BUTTON0=0, BUTTON1};

enum eBUTTONSTATE {BUTTON_UP=0, BUTTON_DOWN};
