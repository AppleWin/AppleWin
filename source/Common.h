#pragma once

const double _14M_NTSC = (157500000.0 / 11.0);	// 14.3181818... * 10^6
const double _14M_PAL = 14.25045e6;				// UTAIIe:3-17
const double CLK_6502_NTSC = (_14M_NTSC * 65.0) / (65.0*14.0+2.0); // 65 cycles per 912 14M clocks
const double CLK_6502_PAL  = (_14M_PAL  * 65.0) / (65.0*14.0+2.0);
//const double CLK_6502 = 23 * 44100;			// 1014300

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
	, MODE_RUNNING  // 6502 is running at normal/full speed (Debugger breakpoints may or may not be active)
	, MODE_DEBUG    // 6502 is paused
	, MODE_STEPPING // 6502 is running at normal/full speed (Debugger breakpoints always active)
	, MODE_BENCHMARK
};

#define  SPEED_MIN         0
#define  SPEED_NORMAL      10
#define  SPEED_MAX         40

#define  DRAW_BACKGROUND    (1 << 0)
#define  DRAW_LEDS          (1 << 1)
#define  DRAW_TITLE         (1 << 2)
#define  DRAW_BUTTON_DRIVES (1 << 3)
#define  DRAW_DISK_STATUS   (1 << 4)

#define  BTN_HELP          0
#define  BTN_RUN           1
#define  BTN_DRIVE1        2
#define  BTN_DRIVE2        3
#define  BTN_DRIVESWAP     4
#define  BTN_FULLSCR       5
#define  BTN_DEBUG         6
#define  BTN_SETUP         7

// TODO: Move to StringTable.h
#define	TITLE_APPLE_2			TEXT("Apple ][ Emulator")
#define	TITLE_APPLE_2_PLUS		TEXT("Apple ][+ Emulator")
#define	TITLE_APPLE_2_JPLUS		TEXT("Apple ][ J-Plus Emulator")
#define	TITLE_APPLE_2E			TEXT("Apple //e Emulator")
#define	TITLE_APPLE_2E_ENHANCED	TEXT("Enhanced Apple //e Emulator")
#define TITLE_APPLE_2C          TEXT("Apple //e Emulator")
#define	TITLE_PRAVETS_82        TEXT("Pravets 82 Emulator")
#define	TITLE_PRAVETS_8M        TEXT("Pravets 8M Emulator")
#define	TITLE_PRAVETS_8A        TEXT("Pravets 8A Emulator")
#define	TITLE_TK3000_2E         TEXT("TK3000 //e Emulator")
#define	TITLE_BASE64A           TEXT("Base64A Emulator")

#define TITLE_PAUSED       TEXT("* PAUSED *")
#define TITLE_STEPPING     TEXT("Stepping")

// Configuration
#define REG_CONFIG						"Configuration"
#define  REGVALUE_APPLE2_TYPE        "Apple2 Type"
#define  REGVALUE_CPU_TYPE           "CPU Type"
#define  REGVALUE_OLD_APPLE2_TYPE    "Computer Emulation"	// Deprecated
#define  REGVALUE_CONFIRM_REBOOT     "Confirm Reboot" // Added at 1.24.1 PageConfig
#define  REGVALUE_FS_SHOW_SUBUNIT_STATUS "Full-screen show subunit status"
#define  REGVALUE_SOUND_EMULATION    "Sound Emulation"
#define  REGVALUE_SPKR_VOLUME        "Speaker Volume"
#define  REGVALUE_MB_VOLUME          "Mockingboard Volume"
#define  REGVALUE_SAVESTATE_FILENAME "Save State Filename"
#define  REGVALUE_SAVE_STATE_ON_EXIT "Save State On Exit"
#define  REGVALUE_HDD_ENABLED        "Harddisk Enable"		// Deprecated from 1.30.5
#define  REGVALUE_JOYSTICK0_EMU_TYPE		"Joystick0 Emu Type v3"	// GH#434: Added at 1.26.3.0 (previously was "Joystick0 Emu Type")
#define  REGVALUE_JOYSTICK1_EMU_TYPE		"Joystick1 Emu Type v3"	// GH#434: Added at 1.26.3.0 (previously was "Joystick1 Emu Type")
#define  REGVALUE_OLD_JOYSTICK0_EMU_TYPE2	"Joystick0 Emu Type"	// GH#434: Deprecated from 1.26.3.0 (previously was "Joystick 0 Emulation")
#define  REGVALUE_OLD_JOYSTICK1_EMU_TYPE2	"Joystick1 Emu Type"	// GH#434: Deprecated from 1.26.3.0 (previously was "Joystick 1 Emulation")
#define  REGVALUE_OLD_JOYSTICK0_EMU_TYPE1	"Joystick 0 Emulation"	// Deprecated from 1.24.0
#define  REGVALUE_OLD_JOYSTICK1_EMU_TYPE1	"Joystick 1 Emulation"	// Deprecated from 1.24.0
#define  REGVALUE_PDL_XTRIM          "PDL X-Trim"
#define  REGVALUE_PDL_YTRIM          "PDL Y-Trim"
#define  REGVALUE_SCROLLLOCK_TOGGLE  "ScrollLock Toggle"
#define  REGVALUE_CURSOR_CONTROL		"Joystick Cursor Control"
#define  REGVALUE_CENTERING_CONTROL		"Joystick Centering Control"
#define  REGVALUE_AUTOFIRE           "Autofire"
#define  REGVALUE_SWAP_BUTTONS_0_AND_1 "Swap buttons 0 and 1"
#define  REGVALUE_MOUSE_CROSSHAIR    "Mouse crosshair"
#define  REGVALUE_MOUSE_RESTRICT_TO_WINDOW "Mouse restrict to window"
#define  REGVALUE_THE_FREEZES_F8_ROM "The Freeze's F8 Rom"
#define  REGVALUE_CIDERPRESSLOC      "CiderPress Location"
#define  REGVALUE_DUMP_TO_PRINTER    "Dump to printer"
#define  REGVALUE_CONVERT_ENCODING   "Convert printer encoding for clones"
#define  REGVALUE_FILTER_UNPRINTABLE "Filter unprintable characters"
#define  REGVALUE_PRINTER_FILENAME   "Printer Filename"
#define  REGVALUE_PRINTER_APPEND     "Append to printer file"
#define  REGVALUE_PRINTER_IDLE_LIMIT "Printer idle limit"
#define  REGVALUE_VIDEO_MODE         "Video Emulation"
#define  REGVALUE_VIDEO_STYLE         "Video Style"			// GH#616: Added at 1.28.2
#define  REGVALUE_VIDEO_HALF_SCAN_LINES "Half Scan Lines"	// GH#616: Deprecated from 1.28.2
#define  REGVALUE_VIDEO_MONO_COLOR      "Monochrome Color"
#define  REGVALUE_VIDEO_REFRESH_RATE    "Video Refresh Rate"
#define  REGVALUE_SERIAL_PORT_NAME   "Serial Port Name"
#define  REGVALUE_ENHANCE_DISK_SPEED "Enhance Disk Speed"
#define  REGVALUE_CUSTOM_SPEED       "Custom Speed"
#define  REGVALUE_EMULATION_SPEED    "Emulation Speed"
#define  REGVALUE_WINDOW_SCALE       "Window Scale"
#define  REGVALUE_UTHERNET_ACTIVE       "Uthernet Active"	// GH#977: Deprecated from 1.30.5
#define  REGVALUE_UTHERNET_INTERFACE    "Uthernet Interface"
#define  REGVALUE_UTHERNET_VIRTUAL_DNS  "Uthernet Virtual DNS"
#define  REGVALUE_SLOT4					"Slot 4"			// GH#977: Deprecated from 1.30.4
#define  REGVALUE_SLOT5					"Slot 5"			// GH#977: Deprecated from 1.30.4
#define  REGVALUE_VERSION				"Version"
#define REG_CONFIG_SLOT_AUX			"Slot Auxiliary"
#define REG_CONFIG_SLOT				"Slot "
#define  REGVALUE_CARD_TYPE			"Card type"
#define  REGVALUE_LAST_DISK_1		"Last Disk Image 1"
#define  REGVALUE_LAST_DISK_2		"Last Disk Image 2"
#define  REGVALUE_LAST_HARDDISK_1	"Last Harddisk Image 1"
#define  REGVALUE_LAST_HARDDISK_2	"Last Harddisk Image 2"

// Preferences 
#define REG_PREFS						"Preferences"
#define REGVALUE_PREF_START_DIR      "Starting Directory"
#define REGVALUE_PREF_WINDOW_X_POS   "Window X-Position"
#define REGVALUE_PREF_WINDOW_Y_POS   "Window Y-Position"
#define REGVALUE_PREF_HDV_START_DIR  "HDV Starting Directory"

#define WM_USER_BENCHMARK	WM_USER+1
#define WM_USER_SAVESTATE	WM_USER+2
#define WM_USER_LOADSTATE	WM_USER+3
#define VK_SNAPSHOT_560		WM_USER+4 // PrintScreen
#define VK_SNAPSHOT_280		WM_USER+5 // PrintScreen+Shift
#define WM_USER_TCP_SERIAL	WM_USER+6
#define WM_USER_BOOT		WM_USER+7
#define WM_USER_FULLSCREEN	WM_USER+8
#define VK_SNAPSHOT_TEXT	WM_USER+9 // PrintScreen+Ctrl

#ifdef _MSC_VER
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

enum eIRQSRC {IS_6522=0, IS_SPEECH, IS_SSC, IS_MOUSE};

//
#define APPLE2P_MASK    0x01
/*
 ][    0
 ][+   1
 //e  10
 //e+ 11
 //c  20
*/
#define APPLE2E_MASK	0x10
#define APPLE2C_MASK	0x20
#define APPLECLONE_MASK	0x100

#define IS_APPLE2		((g_Apple2Type & (APPLE2E_MASK|APPLE2C_MASK)) == 0)
//#define IS_APPLE2E()	(g_Apple2Type & APPLE2E_MASK)	// unused
#define IS_APPLE2C()	(g_Apple2Type & APPLE2C_MASK)
#define IS_CLONE()		(g_Apple2Type & APPLECLONE_MASK)

// NB. These get persisted to the Registry & save-state file, so don't change the values for these enums!
enum eApple2Type {
					A2TYPE_APPLE2=0,
					A2TYPE_APPLE2PLUS,
					A2TYPE_APPLE2JPLUS,
					A2TYPE_APPLE2E=APPLE2E_MASK,
					A2TYPE_APPLE2EENHANCED,
					A2TYPE_UNDEFINED,
					A2TYPE_APPLE2C=APPLE2C_MASK,

					// ][ clones start here:
					A2TYPE_CLONE=APPLECLONE_MASK,
					A2TYPE_PRAVETS8M,								// Apple ][ clone
					A2TYPE_PRAVETS82,								// Apple ][ clone
					A2TYPE_BASE64A,									// Apple ][ clone
					// (Gap for more Apple ][ clones)
					A2TYPE_CLONE_A2_MAX,

					// //e clones start here:
					A2TYPE_CLONE_A2E=A2TYPE_CLONE|APPLE2E_MASK,
					A2TYPE_BAD_PRAVETS82=A2TYPE_CLONE|APPLE2E_MASK,	// Wrongly tagged as Apple //e clone (< AppleWin 1.26)
					A2TYPE_BAD_PRAVETS8M,							// Wrongly tagged as Apple //e clone (< AppleWin 1.26)
					A2TYPE_PRAVETS8A,								// Apple //e clone
					A2TYPE_TK30002E,								// Apple //e enhanced clone
					// (Gap for more Apple //e clones)
					A2TYPE_MAX
				};

inline bool IsApple2Original(eApple2Type type)		// Apple ][
{
	return type == A2TYPE_APPLE2;
}

inline bool IsApple2Plus(eApple2Type type)			// Apple ][,][+,][J-Plus
{
	return ((type & (APPLE2E_MASK|APPLE2C_MASK)) == 0) && !(type & APPLECLONE_MASK);
}

inline bool IsApple2JPlus(eApple2Type type)			// Apple ][J-Plus
{
	return type == A2TYPE_APPLE2JPLUS;
}

inline bool IsClone(eApple2Type type)
{
	return (type & APPLECLONE_MASK) != 0;
}

inline bool IsApple2PlusOrClone(eApple2Type type)	// Apple ][,][+,][J-Plus or clone ][,][+
{
	return (type & (APPLE2E_MASK|APPLE2C_MASK)) == 0;
}

inline bool IsAppleIIeOrAbove(eApple2Type type)		// Apple //e,Enhanced//e,//c or clone //e,Enhanced//e
{
	return !IsApple2PlusOrClone(type);
}

inline bool IsAppleIIc(eApple2Type type)			// Apple //c
{
	return (type & A2TYPE_APPLE2C) != 0;
}

extern eApple2Type g_Apple2Type;
inline bool IsEnhancedIIE(void)
{
	return ( (g_Apple2Type == A2TYPE_APPLE2EENHANCED) || (g_Apple2Type == A2TYPE_TK30002E) );
}

inline bool IsEnhancedIIEorIIC(void)
{
	return ( (g_Apple2Type == A2TYPE_APPLE2EENHANCED) || (g_Apple2Type == A2TYPE_TK30002E) || IS_APPLE2C() );
}

inline bool IsCopamBase64A(eApple2Type type)		// Copam Base64A
{
	return type == A2TYPE_BASE64A;
}

inline bool IsPravets(eApple2Type type)
{
	return type == A2TYPE_PRAVETS8M || type == A2TYPE_PRAVETS82 || type == A2TYPE_PRAVETS8A;
}

enum eBUTTON {BUTTON0=0, BUTTON1};

enum eBUTTONSTATE {BUTTON_UP=0, BUTTON_DOWN};

enum {IDEVENT_TIMER_MOUSE=1, IDEVENT_TIMER_100MSEC};
