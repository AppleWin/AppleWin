#pragma once

// Structs used by save-state file

// *** DON'T CHANGE ANY STRUCT WITHOUT CONSIDERING BACKWARDS COMPATIBILITY WITH .AWS FORMAT ***

#define MAKE_VERSION(a,b,c,d) ((a<<24) | (b<<16) | (c<<8) | (d))

#define AW_SS_TAG 'SSWA'	// 'AWSS' = AppleWin SnapShot

typedef struct
{
	DWORD dwTag;		// "AWSS"
	DWORD dwVersion;
	DWORD dwChecksum;
} SS_FILE_HDR;

typedef struct
{
	DWORD dwLength;		// Byte length of this unit struct
	DWORD dwVersion;
} SS_UNIT_HDR;

/////////////////////////////////////////////////////////////////////////////////

const UINT nMemMainSize = 64*1024;
const UINT nMemAuxSize = 64*1024;

typedef struct
{
	BYTE A;
	BYTE X;
	BYTE Y;
	BYTE P;
	BYTE S;
	USHORT PC;
	unsigned __int64 g_nCumulativeCycles;
	// IRQ = OR-sum of all interrupt sources
} SS_CPU6502;

const UINT uRecvBufferSize = 9;

typedef struct
{
	DWORD  baudrate;
	BYTE   bytesize;
	BYTE   commandbyte;
	DWORD  comminactivity;	// If non-zero then COM port open
	BYTE   controlbyte;
	BYTE   parity;
	BYTE   recvbuffer[uRecvBufferSize];
	DWORD  recvbytes;
	BYTE   stopbits;
} SS_IO_Comms;

typedef struct
{
	unsigned __int64 g_nJoyCntrResetCycle;
} SS_IO_Joystick;

typedef struct
{
	DWORD keyboardqueries;
	BYTE nLastKey;
} SS_IO_Keyboard;

//typedef struct
//{
//} SS_IO_Memory;

typedef struct
{
	unsigned __int64 g_nSpkrLastCycle;
} SS_IO_Speaker;

typedef struct
{
	bool bAltCharSet;	// charoffs
	DWORD dwVidMode;
} SS_IO_Video;

typedef struct
{
	DWORD dwMemMode;
	BOOL bLastWriteRam;
	BYTE nMemMain[nMemMainSize];
	BYTE nMemAux[nMemAuxSize];
} SS_BaseMemory;

typedef struct
{
	SS_UNIT_HDR UnitHdr;
	SS_CPU6502 CPU6502;
	SS_IO_Comms Comms;
	SS_IO_Joystick Joystick;
	SS_IO_Keyboard Keyboard;
//	SS_IO_Memory Memory;
	SS_IO_Speaker Speaker;
	SS_IO_Video Video;
	SS_BaseMemory Memory;
} SS_APPLE2_Unit;

/////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	DWORD dwComputerEmulation;
	bool bCustomSpeed;
	DWORD dwEmulationSpeed;
	bool bEnhancedDiskSpeed;
	DWORD dwJoystickType[2];
	bool bMockingboardEnabled;
	DWORD dwMonochromeColor;
	DWORD dwSerialPort;
	DWORD dwSoundType;	// Sound Emulation
	DWORD dwVideoType;	// Video Emulation
} SS_AW_CFG;

typedef struct
{
	char StartingDir[MAX_PATH];
	DWORD dwWindowXpos;
	DWORD dwWindowYpos;
} SS_AW_PREFS;

typedef struct
{
	SS_UNIT_HDR UnitHdr;
	DWORD dwAppleWinVersion;
	SS_AW_PREFS Prefs;
	SS_AW_CFG Cfg;
} SS_APPLEWIN_CONFIG;

/////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	SS_UNIT_HDR UnitHdr;
	DWORD dwType;		// SS_CARDTYPE
	DWORD dwSlot;		// [1..7]
} SS_CARD_HDR;

enum SS_CARDTYPE
{
	CT_Empty = 0,
	CT_Disk2,			// Apple Disk][
	CT_SSC,				// Apple Super Serial Card
	CT_Mockingboard,
	CT_GenericPrinter,
	CT_GenericHDD,		// Hard disk
	CT_GenericClock,
	CT_MouseInterface,
	CT_Z80,
};

/////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	SS_CARD_HDR	Hdr;
} SS_CARD_EMPTY;

/////////////////////////////////////////////////////////////////////////////////

const UINT NIBBLES_PER_TRACK = 0x1A00;

typedef struct
{
	char	szFileName[MAX_PATH];
	int		track;
	int		phase;
	int		byte;
	BOOL	writeprotected;
	BOOL	trackimagedata;
	BOOL	trackimagedirty;
	DWORD	spinning;
	DWORD	writelight;
	int		nibbles;
	BYTE	nTrack[NIBBLES_PER_TRACK];
} DISK2_Unit;

typedef struct
{
	SS_CARD_HDR	Hdr;
	DISK2_Unit	Unit[2];
    WORD    phases;
	WORD	currdrive;
	BOOL	diskaccessed;
	BOOL	enhancedisk;
	BYTE	floppylatch;
	BOOL	floppymotoron;
	BOOL	floppywritemode;
} SS_CARD_DISK2;

/////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	union
	{
		struct
		{
			BYTE l;
			BYTE h;
		};
		USHORT w;
	};
} IWORD;

typedef struct
{
	BYTE ORB;				// $00 - Port B
	BYTE ORA;				// $01 - Port A (with handshaking)
	BYTE DDRB;				// $02 - Data Direction Register B
	BYTE DDRA;				// $03 - Data Direction Register A
	//
	// $04 - Read counter (L) / Write latch (L)
	// $05 - Read / Write & initiate count (H)
	// $06 - Read / Write & latch (L)
	// $07 - Read / Write & latch (H)
	// $08 - Read counter (L) / Write latch (L)
	// $09 - Read counter (H) / Write latch (H)
	IWORD TIMER1_COUNTER;
	IWORD TIMER1_LATCH;
	IWORD TIMER2_COUNTER;
	IWORD TIMER2_LATCH;
	//
	BYTE SERIAL_SHIFT;		// $0A
	BYTE ACR;				// $0B - Auxiliary Control Register
	BYTE PCR;				// $0C - Peripheral Control Register
	BYTE IFR;				// $0D - Interrupt Flag Register
	BYTE IER;				// $0E - Interrupt Enable Register
	BYTE ORA_NO_HS;			// $0F - Port A (without handshaking)
} SY6522;

typedef struct
{
	BYTE DurationPhonome;
	BYTE Inflection;		// I10..I3
	BYTE RateInflection;
	BYTE CtrlArtAmp;
	BYTE FilterFreq;
	//
	BYTE CurrentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
} SSI263A;

typedef struct
{
	SY6522		RegsSY6522;
	BYTE		RegsAY8910[16];
	SSI263A		RegsSSI263;
	BYTE		nAYCurrentRegister;
	bool		bTimer1IrqPending;
	bool		bTimer2IrqPending;
	bool		bSpeechIrqPending;
} MB_Unit;

const UINT MB_UNITS_PER_CARD = 2;

typedef struct
{
	SS_CARD_HDR	Hdr;
	MB_Unit		Unit[MB_UNITS_PER_CARD];
} SS_CARD_MOCKINGBOARD;

/////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	SS_FILE_HDR Hdr;
	SS_APPLE2_Unit Apple2Unit;
//	SS_APPLEWIN_CONFIG AppleWinCfg;
	SS_CARD_EMPTY Empty1;				// Slot1
	SS_CARD_EMPTY Empty2;				// Slot2
	SS_CARD_EMPTY Empty3;				// Slot3
	SS_CARD_MOCKINGBOARD Mockingboard1;	// Slot4
	SS_CARD_MOCKINGBOARD Mockingboard2;	// Slot5
	SS_CARD_DISK2 Disk2;				// Slot6
	SS_CARD_EMPTY Empty7;				// Slot7
} APPLEWIN_SNAPSHOT;

/////////////////////////////////////////////////////////////////////////////////
