#pragma once

const double _M14 = (157500000.0 / 11.0); // 14.3181818... * 10^6
const double CLK_6502 = ((_M14 * 65.0) / 912.0); // 65 cycles per 912 14M clocks
//const double CLK_6502 = 23 * 44100;			// 1014300

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
	, MODE_RUNNING
	, MODE_DEBUG
	, MODE_STEPPING
};

//#define  MODE_LOGO         0
//#define  MODE_PAUSED       1
//#define  MODE_RUNNING      2
//#define  MODE_DEBUG        3
//#define  MODE_STEPPING     4

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

//#define	MAXIMAGES          16

// TODO: Move to StringTable.h
#define	TITLE_APPLE_2			TEXT("Apple ][ Emulator")
#define	TITLE_APPLE_2_PLUS		TEXT("Apple ][+ Emulator")
#define	TITLE_APPLE_2E			TEXT("Apple //e Emulator")
#define	TITLE_APPLE_2E_ENHANCED	TEXT("Enhanced Apple //e Emulator")

#define TITLE_PAUSED       TEXT(" Paused ")
#define TITLE_STEPPING     TEXT("Stepping")

#define  LOAD(a,b) RegLoadValue(TEXT("Configuration"),a,1,b)
#define  SAVE(a,b) RegSaveValue(TEXT("Configuration"),a,1,b)

// Configuration
#define  REGVALUE_APPLE2_TYPE        "Apple2 Type"
#define  REGVALUE_SPKR_VOLUME        "Speaker Volume"
#define  REGVALUE_MB_VOLUME          "Mockingboard Volume"
#define  REGVALUE_SOUNDCARD_TYPE     "Soundcard Type"
#define  REGVALUE_KEYB_BUFFER_ENABLE "Keyboard Buffer Enable"
#define  REGVALUE_SAVESTATE_FILENAME "Save State Filename"
#define  REGVALUE_SAVE_STATE_ON_EXIT "Save State On Exit"
#define  REGVALUE_HDD_ENABLED        "Harddisk Enable"
#define  REGVALUE_HDD_IMAGE1         "Harddisk Image 1"
#define  REGVALUE_HDD_IMAGE2         "Harddisk Image 2"
#define  REGVALUE_PDL_XTRIM          "PDL X-Trim"
#define  REGVALUE_PDL_YTRIM          "PDL Y-Trim"

// Preferences 
#define REGVALUE_PREF_START_DIR TEXT("Starting Directory")

#define WM_USER_BENCHMARK	WM_USER+1
#define WM_USER_RESTART		WM_USER+2
#define WM_USER_SAVESTATE	WM_USER+3
#define WM_USER_LOADSTATE	WM_USER+4

enum eSOUNDCARDTYPE {SC_UNINIT=0, SC_NONE, SC_MOCKINGBOARD, SC_PHASOR};	// Apple soundcard type

typedef BYTE (__stdcall *iofunction)(WORD nPC, WORD nAddr, BYTE nWriteFlag, BYTE nWriteValue, ULONG nCyclesLeft);

typedef struct _IMAGE__ { int unused; } *HIMAGE;

enum eIRQSRC {IS_6522=0, IS_SPEECH, IS_SSC};

//

#define APPLE2E_MASK	0x10
#define APPLE2C_MASK	0x20

#define IS_APPLE2		((g_Apple2Type & (APPLE2E_MASK|APPLE2C_MASK)) == 0)
#define IS_APPLE2E		(g_Apple2Type & APPLE2E_MASK)
#define IS_APPLE2C		(g_Apple2Type & APPLE2C_MASK)

// NB. These get persisted to the Registry, so don't change the values for these enums!
enum eApple2Type {
					A2TYPE_APPLE2=0,
					A2TYPE_APPLE2PLUS,
					A2TYPE_APPLE2E=APPLE2E_MASK,
					A2TYPE_APPLE2EEHANCED,
//					A2TYPE_APPLE2C=APPLE2C_MASK,	// Placeholder
					A2TYPE_MAX
				};