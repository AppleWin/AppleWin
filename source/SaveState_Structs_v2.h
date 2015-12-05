#pragma once

#include "SaveState_Structs_common.h"

// Structs used by save-state file v2

// *** DON'T CHANGE ANY STRUCT WITHOUT CONSIDERING BACKWARDS COMPATIBILITY WITH .AWS FORMAT ***

/////////////////////////////////////////////////////////////////////////////////

#define SS_FILE_VER 2

#define UNIT_APPLE2_VER 1
#define UNIT_SLOTS_VER 1
#define UNIT_CONFIG_VER 1

struct SS_CPU6502_v2
{
	BYTE A;
	BYTE X;
	BYTE Y;
	BYTE P;
	BYTE S;
	USHORT PC;
	unsigned __int64 CumulativeCycles;
	// IRQ = OR-sum of all interrupt sources
};

struct SS_IO_Joystick_v2
{
	unsigned __int64 JoyCntrResetCycle;
	short Joystick0Trim[2];	// [x,y]
	short Joystick1Trim[2];	// [x,y]
};

struct SS_IO_Keyboard_v2
{
	BYTE LastKey;
};

struct SS_IO_Speaker_v2
{
	unsigned __int64 SpkrLastCycle;
};

struct SS_IO_Video_v2
{
	UINT32 AltCharSet;
	UINT32 VideoMode;
	UINT32 CyclesThisVideoFrame;
};

struct SS_BaseMemory_v2
{
	DWORD dwMemMode;
	BOOL bLastWriteRam;
	BYTE IO_SELECT;
	BYTE IO_SELECT_InternalROM;
	UINT ExpansionRomType;
	UINT PeripheralRomSlot;
	BYTE MemMain[nMemMainSize];
};

struct SS_APPLE2_Unit_v2
{
	SS_UNIT_HDR UnitHdr;
	UINT32 Apple2Type;
	SS_CPU6502_v2 CPU6502;
	SS_IO_Joystick_v2 Joystick;
	SS_IO_Keyboard_v2 Keyboard;
	SS_IO_Speaker_v2 Speaker;
	SS_IO_Video_v2 Video;
	SS_BaseMemory_v2 Memory;
};

/////////////////////////////////////////////////////////////////////////////////

#pragma pack(push,4)	// push current alignment to stack & set alignment to 4
						// - need so that 12-byte Hdr doesn't get padded to 16 bytes
						// - NB. take care not to affect the old v2 structs

struct APPLEWIN_SNAPSHOT_v2
{
	SS_FILE_HDR Hdr;
	SS_APPLE2_Unit_v2 Apple2Unit;
//	SS_CARD_EMPTY[8] Slots;				// Slot 0..7 (0=language card for Apple][)
//	SS_CARD_EMPTY AuxSlot;				// Apple//e auxiliary slot (including optional RAMworks memory)
//	SS_APPLEWIN_CONFIG AppleWinCfg;
};

#pragma pack(pop)

/////////////////////////////////////////////////////////////////////////////////

struct SS_AW_CFG
{
	UINT16 AppleWinVersion[4];	// major,minor,fix,fix_minor
	UINT32 VideoMode;
	UINT32 IsHalfScanLines;
	UINT32 IsConfirmReboot;
	UINT32 MonochromeColor;
	UINT32 WindowScale;
	UINT32 CpuSpeed;
	//
	UINT32 JoystickType[2];
	UINT32 IsAllowCursorsToBeRead;
	UINT32 IsAutofire;
	UINT32 IsKeyboardAutocentering;
	UINT32 IsSwapButton0and1;
	UINT32 IsScrollLockToggle;
	UINT32 IsMouseShowCrosshair;
	UINT32 IsMouseRestrictToWindow;
	//
	UINT32 SoundType;
	UINT32 SpeakerVolume;
	UINT32 MockingboardVolume;
	//
	UINT32 IsEnhancedDiskSpeed;
	UINT32 IsSaveStateOnExit;
	//
	UINT32 IsAppendToFile;
	UINT32 IsUsingFreezesF8Rom;
};

struct SS_APPLEWIN_CONFIG
{
	SS_UNIT_HDR UnitHdr;
	SS_AW_CFG Cfg;
};
