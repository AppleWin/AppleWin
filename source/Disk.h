#pragma once

/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "Card.h"
#include "Log.h"
#include "DiskLog.h"
#include "DiskFormatTrack.h"
#include "DiskImage.h"
#include "SynchronousEventManager.h"

enum Drive_e
{
	DRIVE_1 = 0,
	DRIVE_2,
	NUM_DRIVES
};

const bool IMAGE_USE_FILES_WRITE_PROTECT_STATUS = false;
const bool IMAGE_FORCE_WRITE_PROTECTED = true;
const bool IMAGE_DONT_CREATE = false;
const bool IMAGE_CREATE = true;

class FloppyDisk
{
public:
	FloppyDisk()
	{
		clear();
	}

	~FloppyDisk(){}

	void clear()
	{
		m_imagename.clear();
		m_fullname.clear();
		m_strFilenameInZip.clear();
		m_imagehandle = NULL;
		m_bWriteProtected = false;
		//
		m_byte = 0;
		m_nibbles = 0;
		m_bitOffset = 0;
		m_bitCount = 0;
		m_bitMask = 1 << 7;
		m_extraCycles = 0.0;
		m_trackimage = NULL;
		m_trackimagedata = false;
		m_trackimagedirty = false;
		m_longestSyncFFRunLength = 0;
		m_longestSyncFFBitOffsetStart = -1;
		m_initialBitOffset = 0;
		m_revs = 0;
	}

public:
	std::string m_imagename;	// <FILENAME> (ie. no extension)
	std::string m_fullname;	// <FILENAME.EXT> or <FILENAME.zip>  : This is persisted to the snapshot file
	std::string m_strFilenameInZip;					// ""             or <FILENAME.EXT>
	ImageInfo* m_imagehandle;						// Init'd by InsertDisk() -> ImageOpen()
	bool m_bWriteProtected;
	int m_byte;					// byte offset
	int m_nibbles;				// # nibbles in track / Init'd by ReadTrack() -> ImageReadTrack()
	UINT m_bitOffset;			// bit offset
	UINT m_bitCount;			// # bits in track
	BYTE m_bitMask;
	double m_extraCycles;
	LPBYTE m_trackimage;
	bool m_trackimagedata;
	bool m_trackimagedirty;
	UINT m_longestSyncFFRunLength;
	int m_longestSyncFFBitOffsetStart;
	UINT m_initialBitOffset;	// debug
	UINT m_revs;				// debug
};

class FloppyDrive
{
public:
	FloppyDrive()
	{
		clear();
	}

	~FloppyDrive(){}

	void clear()
	{
		m_isConnected = true;
		m_phasePrecise = 0;
		m_phase = 0;
		m_lastStepperCycle = 0;
		m_motorOnCycle = 0;
		m_headWindow = 0;
		m_spinning = 0;
		m_writelight = 0;
		m_disk.clear();
	}

public:
	bool m_isConnected;
	float m_phasePrecise;	// Phase precise to half a phase (aka quarter track)
	int m_phase;			// Integral phase number
	unsigned __int64 m_lastStepperCycle;
	unsigned __int64 m_motorOnCycle;
	BYTE m_headWindow;
	DWORD m_spinning;
	DWORD m_writelight;
	FloppyDisk m_disk;
};

class Disk2InterfaceCard : public Card
{
public:
	Disk2InterfaceCard(UINT slot);
	virtual ~Disk2InterfaceCard(void);

	virtual void Reset(const bool powerCycle);

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Update(const ULONG nExecutedCycles);

	virtual void Destroy(void);		// no, doesn't "destroy" the disk image.  DiskIIManagerShutdown()

	void Boot(void);
	void FlushCurrentTrack(const int drive);

	const std::string & GetFullDiskFilename(const int drive);
	const std::string & DiskGetFullPathName(const int drive);
	const std::string & GetFullName(const int drive);
	const std::string & GetBaseName(const int drive);
	void GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	void GetLightStatus (Disk_Status_e* pDisk1Status, Disk_Status_e* pDisk2Status);

	ImageError_e InsertDisk(const int drive, const std::string& pathname, const bool bForceWriteProtected, const bool bCreateIfNecessary);
	void EjectDisk(const int drive);
	void UnplugDrive(const int drive);

	bool IsConditionForFullSpeed(void);
	void NotifyInvalidImage(const int drive, const std::string & szImageFilename, const ImageError_e Error);

	UINT GetCurrentBitOffset(void);
	UINT GetCurrentFirmware(void) { return m_is13SectorFirmware ? 13 : 16; }
	double GetCurrentExtraCycles(void);
	float GetCurrentPhase(void);
	int GetCurrentDrive(void);
	BYTE GetCurrentShiftReg(void);
	int GetCurrentTrack(void);

	float GetPhase(const int drive);
	int GetTrack(const int drive);
	static std::string FormatIntFracString(float phase, bool hex);
	std::string GetCurrentTrackString(void);
	std::string GetCurrentPhaseString(void);
	LPCTSTR GetCurrentState(Disk_Status_e& eDiskState_);
	bool UserSelectNewDiskImage(const int drive, LPCSTR pszFilename="");
	bool DriveSwap(void);
	bool IsDriveConnected(int drive) { return m_floppyDrive[drive].m_isConnected; }
	void SetFirmware13Sector(void) { m_force13SectorFirmware = true; }

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	void LoadLastDiskImage(const int drive);
	void SaveLastDiskImage(const int drive);

	bool GetProtect(const int drive);
	void SetProtect(const int drive, const bool bWriteProtect);
	bool IsDriveEmpty(const int drive);
	bool IsWozImageInDrive(const int drive);

	bool GetEnhanceDisk(void);
	void SetEnhanceDisk(bool bEnhanceDisk);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

private:
	void ResetSwitches(void);
	void CheckSpinning(const bool stateChanged, const ULONG uExecutedCycles);
	Disk_Status_e GetDriveLightStatus(const int drive);
	bool IsDriveValid(const int drive);
	void EjectDiskInternal(const int drive);
	void AllocTrack(const int drive, const UINT minSize=NIBBLES_PER_TRACK);
	void ReadTrack(const int drive, ULONG uExecutedCycles);
	void WriteTrack(const int drive);
	void ResetLogicStateSequencer(void);
	UINT GetBitCellDelta(const ULONG uExecutedCycles);
	void UpdateBitStreamPosition(FloppyDisk& floppy, const ULONG bitCellDelta);
	void UpdateBitStreamOffsets(FloppyDisk& floppy);
	__forceinline void IncBitStream(FloppyDisk& floppy);
	void DataLatchReadWOZ(WORD pc, WORD addr, UINT bitCellRemainder);
	void DataLoadWriteWOZ(WORD pc, WORD addr, UINT bitCellRemainder);
	void DataShiftWriteWOZ(WORD pc, WORD addr, ULONG uExecutedCycles);
	void SetSequencerFunction(WORD addr, ULONG executedCycles);
	void FindTrackSeamWOZ(FloppyDisk& floppy, float track);
	void DumpTrackWOZ(FloppyDisk floppy);
	bool GetFirmware(WORD lpNameId, BYTE* pDst);
	void InitFirmware(LPBYTE pCxRomPeripheral);
	void UpdateLatchForEmptyDrive(FloppyDrive* pDrive);
	void InsertSyncEvent(void);
	static int SyncEventCallback(int id, int cycles, ULONG uExecutedCycles);
	void ControlStepperDeferred(void);
	void ControlStepperLogging(WORD address, unsigned __int64 cumulativeCycles);

	void PreJitterCheck(int phase, BYTE latch);
	void AddJitter(int phase, FloppyDisk& floppy);
	void AddTrackSeamJitter(float phasePrecise, FloppyDisk& floppy);

	void SaveSnapshotFloppy(YamlSaveHelper& yamlSaveHelper, UINT unit);
	void SaveSnapshotDriveUnit(YamlSaveHelper& yamlSaveHelper, UINT unit);
	bool LoadSnapshotFloppy(YamlLoadHelper& yamlLoadHelper, UINT unit, UINT version, std::vector<BYTE>& track);
	bool LoadSnapshotDriveUnitv3(YamlLoadHelper& yamlLoadHelper, UINT unit, UINT version, std::vector<BYTE>& track);
	bool LoadSnapshotDriveUnitv4(YamlLoadHelper& yamlLoadHelper, UINT unit, UINT version, std::vector<BYTE>& track);
	void LoadSnapshotDriveUnit(YamlLoadHelper& yamlLoadHelper, UINT unit, UINT version);

	void __stdcall ControlStepper(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	void __stdcall ControlMotor(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	bool __stdcall Enable(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	void __stdcall ReadWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles);
	void __stdcall DataLatchReadWriteWOZ(WORD pc, WORD addr, BYTE bWrite, ULONG uExecutedCycles);
	void __stdcall LoadWriteProtect(WORD, WORD, BYTE write, BYTE value, ULONG);
	void __stdcall SetReadMode(WORD, WORD, BYTE, BYTE, ULONG);
	void __stdcall SetWriteMode(WORD, WORD, BYTE, BYTE, ULONG uExecutedCycles);

#if LOG_DISK_NIBBLES_WRITE
	bool LogWriteCheckSyncFF(ULONG& uCycleDelta);
#endif

	//

	static const UINT DISK2_FW_SIZE = 256;
	BYTE m_13SectorFirmware[DISK2_FW_SIZE];
	BYTE m_16SectorFirmware[DISK2_FW_SIZE];
	bool m_is13SectorFirmware;
	bool m_force13SectorFirmware;

	WORD m_currDrive;
	FloppyDrive m_floppyDrive[NUM_DRIVES];
	BYTE m_floppyLatch;
	BOOL m_floppyMotorOn;

	// Although the magnets are a property of the drive, their state is a property of the controller card,
	// since the magnets will only be on for whichever of the 2 drives is currently selected.
	WORD m_magnetStates;	// state bits for stepper motor magnet states (phases 0 - 3)

	bool m_saveDiskImage;
	unsigned __int64 m_diskLastCycle;
	unsigned __int64 m_diskLastReadLatchCycle;
	FormatTrack m_formatTrack;
	bool m_enhanceDisk;

	static const UINT SPINNING_CYCLES = 1000*1000;		// 1M cycles = ~1.000s
	static const UINT WRITELIGHT_CYCLES = 1000*1000;	// 1M cycles = ~1.000s
	static const UINT MOTOR_ON_UNTIL_LSS_STABLE_CYCLES = 0x2EC;	// ~0x2EC-0x990 cycles (depending on card). See GH#864

	// Logic State Sequencer (for WOZ):
	BYTE m_shiftReg;
	int m_latchDelay;
	bool m_writeStarted;

	enum SEQFUNC {readSequencing=0, dataShiftWrite, checkWriteProtAndInitWrite, dataLoadWrite};	// UTAIIe 9-14
	union SEQUENCER_FUNCTION
	{
		struct
		{
			UINT writeMode : 1;	// $C08E/F,X
			UINT loadMode : 1;	// $C08C/D,X
		};
		SEQFUNC function;
	};

	SEQUENCER_FUNCTION m_seqFunc;
	UINT m_dbgLatchDelayedCnt;

	bool m_deferredStepperEvent;
	WORD m_deferredStepperAddress;
	unsigned __int64 m_deferredStepperCumulativeCycles;
	SyncEvent m_syncEvent;

	// Jitter (GH#930)
	static const BYTE m_T00S00Pattern[];
	UINT m_T00S00PatternIdx;
	bool m_foundT00S00Pattern;

	// Debug:
#if LOG_DISK_NIBBLES_USE_RUNTIME_VAR
	bool m_bLogDisk_NibblesRW;	// From VS Debugger, change this to true/false during runtime for precise nibble logging
#endif
#if LOG_DISK_NIBBLES_WRITE
	UINT64 m_uWriteLastCycle;
	UINT m_uSyncFFCount;
#endif
};
