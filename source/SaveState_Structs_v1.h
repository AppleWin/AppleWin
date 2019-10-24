#pragma once

#include "DiskDefs.h"
#include "SaveState_Structs_common.h"

// Structs used by save-state file v1

// *** DON'T CHANGE ANY STRUCT WITHOUT CONSIDERING BACKWARDS COMPATIBILITY WITH .AWS FORMAT ***

/////////////////////////////////////////////////////////////////////////////////

struct SS_CPU6502
{
	BYTE A;
	BYTE X;
	BYTE Y;
	BYTE P;
	BYTE S;
	USHORT PC;
	unsigned __int64 nCumulativeCycles;
	// IRQ = OR-sum of all interrupt sources
};

const UINT uRecvBufferSize = 9;

struct SS_IO_Comms
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
};

struct SS_IO_Joystick
{
	unsigned __int64 nJoyCntrResetCycle;
};

struct SS_IO_Keyboard
{
	DWORD keyboardqueries;
	BYTE nLastKey;
};

struct SS_IO_Speaker
{
	unsigned __int64 nSpkrLastCycle;
};

struct SS_IO_Video
{
	bool bAltCharSet;	// charoffs
	DWORD dwVidMode;
};

struct SS_BaseMemory
{
	DWORD dwMemMode;
	BOOL bLastWriteRam;
	BYTE nMemMain[nMemMainSize];
	BYTE nMemAux[nMemAuxSize];
};

struct SS_APPLE2_Unit
{
	SS_UNIT_HDR UnitHdr;
	SS_CPU6502 CPU6502;
	SS_IO_Comms Comms;
	SS_IO_Joystick Joystick;
	SS_IO_Keyboard Keyboard;
	SS_IO_Speaker Speaker;
	SS_IO_Video Video;
	SS_BaseMemory Memory;
};

/////////////////////////////////////////////////////////////////////////////////

struct DISK2_Unit
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
	BYTE	nTrack[NIBBLES_PER_TRACK_NIB];
};

struct SS_CARD_DISK2
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
};

/////////////////////////////////////////////////////////////////////////////////

struct MB_Unit_v1
{
	SY6522		RegsSY6522;
	BYTE		RegsAY8910[16];
	SSI263A		RegsSSI263;
	BYTE		nAYCurrentRegister;
	bool		bTimer1IrqPending;
	bool		bTimer2IrqPending;
	bool		bSpeechIrqPending;
};

const UINT MB_UNITS_PER_CARD_v1 = 2;

struct SS_CARD_MOCKINGBOARD_v1
{
	SS_CARD_HDR	Hdr;
	MB_Unit_v1	Unit[MB_UNITS_PER_CARD_v1];
};

/////////////////////////////////////////////////////////////////////////////////

struct APPLEWIN_SNAPSHOT_v1
{
	SS_FILE_HDR Hdr;
	SS_APPLE2_Unit Apple2Unit;
	SS_CARD_EMPTY Empty1;					// Slot1
	SS_CARD_EMPTY Empty2;					// Slot2
	SS_CARD_EMPTY Empty3;					// Slot3
	SS_CARD_MOCKINGBOARD_v1 Mockingboard1;	// Slot4
	SS_CARD_MOCKINGBOARD_v1 Mockingboard2;	// Slot5
	SS_CARD_DISK2 Disk2;					// Slot6
	SS_CARD_EMPTY Empty7;					// Slot7
};

const UINT kSnapshotSize_v1 = 145400;		// Const size for v1
