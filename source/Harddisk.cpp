/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski

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

/* Description: Hard drive emulation
 *
 * Author: Copyright (c) 2005, Robert Hoem
 */

#include "StdAfx.h"

#include "Harddisk.h"
#include "Core.h"
#include "Interface.h"
#include "CPU.h"
#include "DiskImage.h"	// ImageError_e, Disk_Status_e
#include "Memory.h"
#include "Registry.h"
#include "SaveState.h"
#include "YamlHelper.h"

#include "Debugger/Debug.h"
#include "../resource/resource.h"

/*
Memory map ProDOS BLK device (IO addr + s*$10):
. "hddrvr" v1 and v2 firmware

	C080	(r)   EXECUTE AND RETURN STATUS
	C081	(r)   STATUS (or ERROR): b7=busy, b0=error
	C082	(r/w) COMMAND
	C083	(r/w) UNIT NUMBER
	C084	(r/w) LOW BYTE OF MEMORY BUFFER
	C085	(r/w) HIGH BYTE OF MEMORY BUFFER
	C086	(r/w) LOW BYTE OF BLOCK NUMBER
	C087	(r/w) HIGH BYTE OF BLOCK NUMBER
	C088	(r)   NEXT BYTE (legacy read-only port - still supported)
	C089	(r)   LOW BYTE OF DISK IMAGE SIZE IN BLOCKS
	C08A	(r)   HIGH BYTE OF DISK IMAGE SIZE IN BLOCKS

Firmware notes:
. ROR ABS16,X and ROL ABS16,X - only used for $C081+s*$10 STATUS register:
    6502:  double read (old data), write (old data), write (new data). The writes are harmless as writes to STATUS are ignored.
    65C02: double read (old data), write (new data). The write is harmless as writes to STATUS are ignored.
. STA ABS16,X does a false-read. This is harmless for writable I/O registers, since the false-read has no side effect.

---

Memory map SmartPort device (IO addr + s*$10):
. "hdc-smartport" firmware
. I/O basically compatible with older "hddrvr" firmware

	C080	(r)   EXECUTE AND RETURN STATUS; subsequent reads just return STATUS (need to write COMMAND again for EXECUTE)
	C081	(r)   STATUS : b7=busy, b0=error
	C082	(w)   COMMAND : BLK = $00 status, $01 read, $02 write. SP = $80 status, $81 read, $82 write,
	C083	(w)   UNIT NUMBER : BLK = DSSS0000 if SSS != n from CnXX, add 2 to D (4 drives support). SP = $00,$01.....
	C084	(w)   LOW BYTE OF MEMORY BUFFER
	C085	(w)   HIGH BYTE OF MEMORY BUFFER
	C086	(w)   STATUS CODE : write SP status code $00(device status), $03(device info block)
	C086	(w)   LOW BYTE OF BLOCK NUMBER : BLK = 16 bit value. SP = 24 bit value
	C087	(w)   MIDDLE BYTE OF BLOCK NUMBER
	C088	(w)   HIGH BYTE OF BLOCK NUMBER (SP only)
;	C088	(r)   NEXT BYTE (legacy read-only port - still supported)
	C089	(r)   LOW BYTE OF DISK IMAGE SIZE IN BLOCKS
	C08A	(r)   HIGH BYTE OF DISK IMAGE SIZE IN BLOCKS
	C089	(w)   a 6-deep FIFO to write: command, unitNum, memPtr(2), blockNum(2); for BLK driver
	C08A	(w)   a 7-deep FIFO to write: command, unitNum, memPtr(2), blockNum(3); for SP: first byte gets OR'd with $80 (ie. to indicate it's an SP command)
*/

/*
Hard drive emulation in AppleWin - for the HDDRVR v1 & v2 firmware

Concept
    To emulate a 32MiB hard drive connected to an Apple IIe via AppleWin.
    Designed to work with Autoboot ROM and ProDOS.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDOS). To create
      an empty .HDV file, just create a 0 byte file.
      Use the -harddisknumblocks n command line option to set the disk size 
      returned by ProDOS status calls.

  2. Emulation code
      There are 4 commands ProDOS will send to a block device.
      Listed below are each command and how it's handled:

      1. STATUS
          In the emulation's case, returns only a DEVICE OK (0), DEVICE I/O ERROR ($27) or DEVICE NOT CONNECTED ($28)
          DEVICE NOT CONNECTED only returned if no HDV file is selected.

      2. READ
          Loads requested block into a 512 byte buffer by attempting to seek to
            location in HDV file.
          If seek fails, returns a DEVICE I/O ERROR.  Resets m_buf_ptr used by legacy HD_NEXTBYTE
          Copies requested block from a 512 byte buffer to the Apple's memory.
          Sets STATUS.busy=1 until the DMA operation completes.
          Returns a DEVICE OK if read was successful, or a DEVICE I/O ERROR otherwise.

      3. WRITE
          Copies requested block from the Apple's memory to a 512 byte buffer
            then attempts to seek to requested block.
          If the seek fails (usually because the seek is beyond the EOF for the
            HDV file), the emulation will attempt to "grow" the HDV file to accommodate.
            Once the file can accommodate, or if the seek did not fail, the buffer is
            written to the HDV file.  NOTE: A2PC will grow *AND* shrink the HDV file.
		  Sets STATUS.busy=1 until the DMA operation completes.
          I didn't see the point in shrinking the file as this behaviour would require
            patching prodos (to detect DELETE FILE calls).

      4. FORMAT
          This is used for low level formatting of the device. (GH#88)
            (Also in the case of a tape or SCSI drive, perhaps.)

  3. Bugs
      The only thing I've noticed is that Copy II+ 7.1 seems to crash or stall
      occasionally when trying to calculate how many free blocks are available
      when running a catalog.  This might be due to the great number of blocks
      available.  Also, DDD pro will not optimise the disk correctly (it's
      doing a disk defragment of some sort, and when it requests a block outside
      the range of the image file, it starts getting I/O errors), so don't
      bother.  Any program that preforms a read before write to an "unwritten"
      block (a block that should be located beyond the EOF of the .HDV, which is
      valid for writing but not for reading until written to) will fail with I/O
      errors (although these are few and far between).

      I'm sure there are programs out there that may try to use the I/O ports in
      ways they weren't designed (like telling Ultima 5 that you have a Phasor
      sound card in slot 7 is a generally bad idea) will cause problems.
*/



HarddiskInterfaceCard::HarddiskInterfaceCard(UINT slot) :
	Card(CT_GenericHDD, slot), m_userNumBlocks(0), m_isFirmwareV1or2(false), m_useHdcFirmwareV1(false), m_useHdcFirmwareV2(false), m_useHdcFirmwareMode(HdcDefault)
{
	if (m_slot != SLOT5 && m_slot != SLOT7)	// fixme
		ThrowErrorInvalidSlot();

	m_unitNum = (HARDDISK_1 << 7) | (m_slot << 4);	// b7=unit, b6:4=slot

	// The HDD interface has a single Command register for both drives:
	// . ProDOS will write to Command before switching drives
	m_command = 0;

	m_fifoIdx = 0;

	// SmartPort Status cmd's Status code
	m_statusCode = 0;

	// SmartPort Controller is always loaded
	m_smartPortController.m_imageloaded = true;

	// Interface busy doing DMA for r/w when current cycle is earlier than this cycle
	m_notBusyCycle = 0;

	m_saveDiskImage = true;	// Save the DiskImage name to Registry

	m_saveStateFirmwareV1 = false;
	m_saveStateFirmwareV2 = false;
	m_saveStateFirmwareValid = false;
	memset(m_saveStateFirmware, 0, sizeof(m_saveStateFirmware));
}

HarddiskInterfaceCard::~HarddiskInterfaceCard(void)
{
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
		CleanupDriveInternal(i);
}

void HarddiskInterfaceCard::Reset(const bool powerCycle)
{
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
		m_hardDiskDrive[i].m_error = 0;

	m_fifoIdx = 0;
}

//===========================================================================

void HarddiskInterfaceCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	const uint32_t HARDDISK_FW_SIZE = APPLE_SLOT_SIZE;
	WORD id = IDR_HDC_SMARTPORT_FW;	// If not enhanced //e, then modify the firmware later

	// Use any cmd line override
	if (m_useHdcFirmwareV1 || m_saveStateFirmwareV1) id = IDR_HDDRVR_FW;
	else if (m_useHdcFirmwareV2 || m_saveStateFirmwareV2) id = IDR_HDDRVR_V2_FW;

	m_saveStateFirmwareV1 = false;
	m_saveStateFirmwareV2 = false;

	bool allowFirmwareMods = false;
	BYTE* pData = NULL;
	if (!m_saveStateFirmwareValid)
	{
		pData = GetFrame().GetResource(id, "FIRMWARE", HARDDISK_FW_SIZE);
		if (pData == NULL)
			return;
		if (id == IDR_HDDRVR_FW || id == IDR_HDDRVR_V2_FW)
			m_isFirmwareV1or2 = true;
		allowFirmwareMods = true;
	}
	else
	{
		m_saveStateFirmwareValid = false;
		pData = m_saveStateFirmware;
	}

	BYTE* pFirmwareBase = pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE;
	memcpy(pFirmwareBase, pData, HARDDISK_FW_SIZE);

	if (allowFirmwareMods)
	{
		if (m_useHdcFirmwareMode == HdcBlockMode2Devices || m_useHdcFirmwareMode == HdcBlockMode4Devices)
		{
			pFirmwareBase[0x07] = 0x3C;	// Block mode (not SmartPort)
			const BYTE numDrives = (m_useHdcFirmwareMode == HdcBlockMode2Devices) ? 1 : 3;
			pFirmwareBase[0xFE] = (pFirmwareBase[0xFE] & 0xCF) | (numDrives << 4);	// 2 or 4 drives
		}
		else if (!IsEnhancedIIE())
		{
			if (m_useHdcFirmwareMode != HdcSmartPort)
				pFirmwareBase[0x07] = 0x3C;	// Block mode (not SmartPort)
		}
	}

	RegisterIoHandler(m_slot, IORead, IOWrite, NULL, NULL, this, NULL);
}

//===========================================================================

void HarddiskInterfaceCard::CleanupDriveInternal(const int iDrive)
{
	if (m_hardDiskDrive[iDrive].m_imagehandle)
	{
		ImageClose(m_hardDiskDrive[iDrive].m_imagehandle);
		m_hardDiskDrive[iDrive].m_imagehandle = NULL;
	}

	m_hardDiskDrive[iDrive].m_imageloaded = false;

	m_hardDiskDrive[iDrive].m_imagename.clear();
	m_hardDiskDrive[iDrive].m_fullname.clear();
	m_hardDiskDrive[iDrive].m_strFilenameInZip.clear();
}

void HarddiskInterfaceCard::CleanupDrive(const int iDrive)
{
	CleanupDriveInternal(iDrive);

	SaveLastDiskImage(iDrive);
}

//===========================================================================

void HarddiskInterfaceCard::NotifyInvalidImage(const std::string & szImageFilename)
{
	// TC: TO DO - see Disk2InterfaceCard::NotifyInvalidImage()

	std::string strText = StrFormat("Unable to open the file %s.",
									szImageFilename.c_str());

	GetFrame().FrameMessageBox(strText.c_str(),
							   g_pAppTitle.c_str(),
							   MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}

//===========================================================================

void HarddiskInterfaceCard::LoadLastDiskImage(const int drive)
{
	_ASSERT(drive >= HARDDISK_1 && drive < NUM_HARDDISKS);

	const std::string regKey = std::string(REGVALUE_LAST_HARDDISK_) + (char)('1' + drive);
	char pathname[MAX_PATH];

	std::string regSection = RegGetConfigSlotSection(m_slot);
	if (RegLoadString(regSection.c_str(), regKey.c_str(), TRUE, pathname, MAX_PATH, "") && (pathname[0] != 0))
	{
		m_saveDiskImage = false;
		bool res = Insert(drive, pathname);
		m_saveDiskImage = true;

		if (!res)
		{
			NotifyInvalidImage(pathname);
			CleanupDrive(drive);
		}
	}
}

//===========================================================================

void HarddiskInterfaceCard::SaveLastDiskImage(const int drive)
{
	_ASSERT(drive >= HARDDISK_1 && drive < NUM_HARDDISKS);

	if (!m_saveDiskImage)
		return;

	std::string regSection = RegGetConfigSlotSection(m_slot);
	RegSaveValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, CT_GenericHDD);

	const std::string regKey = std::string(REGVALUE_LAST_HARDDISK_) + (char)('1' + drive);
	const std::string& pathName = HarddiskGetFullPathName(drive);

	RegSaveString(regSection.c_str(), regKey.c_str(), TRUE, pathName);

	//

	// For now, only update 'HDV Starting Directory' for slot7 & drive1
	// . otherwise you'll get inconsistent results if you set drive1, then drive2 (and the images were in different folders)
	if (m_slot != SLOT7 || drive != HARDDISK_1)
		return;

	const size_t slash = pathName.find_last_of(PATH_SEPARATOR);
	if (slash != std::string::npos)
	{
		const std::string dirName = pathName.substr(0, slash + 1);
		RegSaveString(REG_PREFS, REGVALUE_PREF_HDV_START_DIR, 1, dirName);
	}
}

//===========================================================================

const std::string& HarddiskInterfaceCard::GetFullName(const int iDrive)
{
	return m_hardDiskDrive[iDrive].m_fullname;
}

const std::string& HarddiskInterfaceCard::HarddiskGetFullPathName(const int iDrive)
{
	return ImageGetPathname(m_hardDiskDrive[iDrive].m_imagehandle);
}

const std::string& HarddiskInterfaceCard::DiskGetBaseName(const int iDrive)
{
	return m_hardDiskDrive[iDrive].m_imagename;
}

void HarddiskInterfaceCard::GetFilenameAndPathForSaveState(std::string& filename, std::string& path)
{
	filename = "";
	path = "";

	for (UINT i = 0; i < NUM_HARDDISKS; i++)
	{
		if (!m_hardDiskDrive[i].m_imageloaded)
			continue;

		filename = DiskGetBaseName(i);
		std::string pathname = HarddiskGetFullPathName(i);

		int idx = pathname.find_last_of(PATH_SEPARATOR);
		if (idx >= 0 && idx+1 < (int)pathname.length())	// path exists?
		{
			path = pathname.substr(0, idx+1);
			return;
		}

		_ASSERT(0);
		break;
	}
}

//===========================================================================

void HarddiskInterfaceCard::Destroy(void)
{
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
	{
		m_saveDiskImage = false;
		CleanupDrive(i);
	}

	m_saveDiskImage = true;
}

//===========================================================================

// Pre: pathname likely to include path (but can also just be filename)
bool HarddiskInterfaceCard::Insert(const int iDrive, const std::string& pathname)
{
	if (pathname.empty())
		return false;

	if (m_hardDiskDrive[iDrive].m_imageloaded)
		Unplug(iDrive);

	const DWORD dwAttributes = GetFileAttributes(pathname.c_str());
	if (dwAttributes == INVALID_FILE_ATTRIBUTES)
		m_hardDiskDrive[iDrive].m_bWriteProtected = false;	// File doesn't exist - so ImageOpen() below will fail
	else
		m_hardDiskDrive[iDrive].m_bWriteProtected = (dwAttributes & FILE_ATTRIBUTE_READONLY) ? true : false;

	// Check if image is being used by the other HDD, and unplug it in order to be swapped
	{
		const std::string & pszOtherPathname = HarddiskGetFullPathName(!iDrive);

		char szCurrentPathname[MAX_PATH]; 
		DWORD uNameLen = GetFullPathName(pathname.c_str(), MAX_PATH, szCurrentPathname, NULL);
		if (uNameLen == 0 || uNameLen >= MAX_PATH)
			strcpy_s(szCurrentPathname, MAX_PATH, pathname.c_str());

		if (!strcmp(pszOtherPathname.c_str(), szCurrentPathname))
		{
			Unplug(!iDrive);
			GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);
		}
	}

	const bool bCreateIfNecessary = false;		// NB. Don't allow creation of HDV files
	const bool bExpectFloppy = false;
	const bool bIsHarddisk = true;
	ImageError_e Error = ImageOpen(pathname,
		&m_hardDiskDrive[iDrive].m_imagehandle,
		&m_hardDiskDrive[iDrive].m_bWriteProtected,
		bCreateIfNecessary,
		m_hardDiskDrive[iDrive].m_strFilenameInZip,	// TODO: Use this
		bExpectFloppy);

	m_hardDiskDrive[iDrive].m_imageloaded = (Error == eIMAGE_ERROR_NONE);

	m_hardDiskDrive[iDrive].m_status_next = DISK_STATUS_OFF;
	m_hardDiskDrive[iDrive].m_status_prev = DISK_STATUS_OFF;

	if (Error == eIMAGE_ERROR_NONE)
	{
		GetImageTitle(pathname.c_str(), m_hardDiskDrive[iDrive].m_imagename, m_hardDiskDrive[iDrive].m_fullname);
		Snapshot_UpdatePath();
	}

	SaveLastDiskImage(iDrive);

	return m_hardDiskDrive[iDrive].m_imageloaded;
}

//-----------------------------------------------------------------------------

bool HarddiskInterfaceCard::SelectImage(const int drive, LPCSTR pszFilename)
{
	char directory[MAX_PATH];
	char filename[MAX_PATH];

	StringCbCopy(filename, MAX_PATH, pszFilename);

	RegLoadString(REG_PREFS, REGVALUE_PREF_HDV_START_DIR, 1, directory, MAX_PATH, "");
	std::string title = StrFormat("Select HDV Image For HDD %d", drive + 1);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = GetFrame().g_hFrameWindow;
	ofn.hInstance       = GetFrame().g_hInstance;
	ofn.lpstrFilter     = "Hard Disk Images (*.hdv,*.po,*.2mg,*.2img,*.gz,*.zip)\0*.hdv;*.po;*.2mg;*.2img;*.gz;*.zip\0"
						  "All Files\0*.*\0";
	ofn.lpstrFile       = filename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = directory;
	ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;	// Don't allow creation & hide the read-only checkbox
	ofn.lpstrTitle      = title.c_str();

	bool bRes = false;

	if (GetOpenFileName(&ofn))
	{
		std::string openFilename = filename;
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			openFilename += ".hdv";
		
		if (Insert(drive, openFilename))
		{
			bRes = true;
		}
		else
		{
			NotifyInvalidImage(openFilename);
		}
	}

	return bRes;
}

bool HarddiskInterfaceCard::Select(const int iDrive)
{
	return SelectImage(iDrive, "");
}

//===========================================================================

void HarddiskInterfaceCard::Unplug(const int iDrive)
{
	if (m_hardDiskDrive[iDrive].m_imageloaded)
	{
		CleanupDrive(iDrive);
		Snapshot_UpdatePath();
	}
}

//===========================================================================

#if 0	// Enable HDD command logging
#define LOG_DISK(format, ...) LOG(format, __VA_ARGS__)
#define DEBUG_SKIP_BUSY_STATUS 1
#else
#define LOG_DISK(...)
#define DEBUG_SKIP_BUSY_STATUS 0
#endif


// ProDOS BLK & SmartPort commands
//
const UINT BLK_Cmd_Status		= 0x00;
const UINT BLK_Cmd_Read			= 0x01;
const UINT BLK_Cmd_Write		= 0x02;
const UINT BLK_Cmd_Format		= 0x03;
//
const UINT SP_Cmd_base			= 0x80;
const UINT SP_Cmd_extended		= 0x40;
const UINT SP_Cmd_status		= SP_Cmd_base + 0x00;
const UINT SP_Cmd_status_STATUS	= 0x00;
const UINT SP_Cmd_status_GETDCB	= 0x01;
const UINT SP_Cmd_status_GETNL	= 0x02;
const UINT SP_Cmd_status_GETDIB	= 0x03;
const UINT SP_Cmd_readblock		= SP_Cmd_base + 0x01;
const UINT SP_Cmd_writeblock	= SP_Cmd_base + 0x02;
const UINT SP_Cmd_format		= SP_Cmd_base + 0x03;
const UINT SP_Cmd_control		= SP_Cmd_base + 0x04;
const UINT SP_Cmd_init			= SP_Cmd_base + 0x05;
const UINT SP_Cmd_open			= SP_Cmd_base + 0x06;
const UINT SP_Cmd_close			= SP_Cmd_base + 0x07;
const UINT SP_Cmd_read			= SP_Cmd_base + 0x08;
const UINT SP_Cmd_write			= SP_Cmd_base + 0x09;
const UINT SP_Cmd_busyStatus	= SP_Cmd_base + 0x3F;	// AppleWin vendor-specific command

#define DEVICE_OK				0x00
#define BUSERR					0x06
#define BADCTL					0x21
#define DEVICE_IO_ERROR			0x27
#define DEVICE_NOT_CONNECTED	0x28	// No device detected/connected
#define NOWRITE					0x2B	// Disk write protected
#define ERRORCODE_MASK			0x3F	// limit to just 6 bits (as 'error' byte uses b0 & b7 for other status)

#define STATUS_OK				0x00
#define STATUS_ERROR			0x01
#define STATUS_BUSY				0x80
#define SET_STATUS_ERROR(err)	(((err)<<1)|STATUS_ERROR)

BYTE __stdcall HarddiskInterfaceCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	HarddiskInterfaceCard* pCard = (HarddiskInterfaceCard*)MemGetSlotParameters(slot);

	HardDiskDrive* pHDD = NULL;
	const BYTE addrIdx = addr & 0xF;
	if (addrIdx != 0x2 && addrIdx != 0x3)	// GetUnit() depends on m_command & m_unitNum
	{
		pHDD = pCard->GetUnit();
		if (pHDD == NULL)
			return SET_STATUS_ERROR(DEVICE_NOT_CONNECTED);
	}

	CpuCalcCycles(nExecutedCycles);

	BYTE r = STATUS_OK;

	switch (addrIdx)
	{
	case 0x0:	// EXECUTE & RETURN STATUS
		r = pCard->CmdExecute(pHDD, nExecutedCycles);
		pCard->m_command = (pCard->m_command & SP_Cmd_base) ? SP_Cmd_busyStatus : BLK_Cmd_Status;	// Subsequent reads from IO addr 0x0 just executes 'Status' cmd
		_ASSERT(pCard->m_fifoIdx == 0);
		pCard->m_fifoIdx = 0;
		break;
	case 0x1:	// STATUS
		r = pCard->CmdStatus(pHDD);
		break;
	case 0x2:
		r = pCard->m_command;
		break;
	case 0x3:
		r = pCard->m_unitNum;
		break;
	case 0x4:
		r = (BYTE)(pHDD->m_memblock & 0x00FF);
		break;
	case 0x5:
		r = (BYTE)((pHDD->m_memblock & 0xFF00) >> 8);
		break;
	case 0x6:
		r = (BYTE)(pHDD->m_diskblock & 0x00FF);
		break;
	case 0x7:
		r = (BYTE)((pHDD->m_diskblock & 0xFF00) >> 8);
		break;
	case 0x8:	// Legacy: continue to support this I/O port for old HDC firmware
		r = pHDD->m_buf[pHDD->m_buf_ptr];
		if (pHDD->m_buf_ptr < sizeof(pHDD->m_buf)-1)
			pHDD->m_buf_ptr++;
		break;
	case 0x9:
		if (pHDD->m_imageloaded)
			r = (BYTE)(pCard->GetImageSizeInBlocks(pHDD->m_imagehandle, true) & 0x00ff);
		else
			r = 0;
		break;
	case 0xa:
		if (pHDD->m_imageloaded)
			r = (BYTE)((pCard->GetImageSizeInBlocks(pHDD->m_imagehandle, true) & 0xff00) >> 8);
		else
			r = 0;
		break;
	default:
		LOG_DISK("slot-%d, Bad IORead(), io reg=$%1X, PC=$%04X\n", pCard->m_slot, addrIdx, pc);
		pHDD->m_status_next = DISK_STATUS_OFF;
		r = IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	}

	if (pHDD)
		pCard->UpdateLightStatus(pHDD);

	return r;
}

BYTE HarddiskInterfaceCard::CmdExecute(HardDiskDrive* pHDD, const ULONG nExecutedCycles)
{
	LOG_DISK("slot-%d, HDD-%d(%02X): Cmd=%02X ", m_slot, (m_command & SP_Cmd_base) ? m_unitNum : GetProDOSBlockDeviceUnit(), m_unitNum, m_command);

	if (!pHDD->m_imageloaded && m_command != BLK_Cmd_Status && m_command != SP_Cmd_status)
	{
		pHDD->m_status_next = DISK_STATUS_OFF;
		pHDD->m_error = DEVICE_NOT_CONNECTED;	// GH#452
		return CmdStatus(pHDD);
	}

	if ((m_command == SP_Cmd_readblock || m_command == SP_Cmd_writeblock) && pHDD->m_diskblock > kHarddiskMaxNumBlocks)
	{
		pHDD->m_status_next = DISK_STATUS_OFF;
		pHDD->m_error = DEVICE_IO_ERROR;
		return CmdStatus(pHDD);
	}

	const UINT CYCLES_FOR_DMA_RW_BLOCK = HD_BLOCK_SIZE;
	const UINT CYCLES_FOR_FORMATTING_1_BLOCK = 100;	// Arbitrary
	const UINT PAGE_SIZE = 256;

	pHDD->m_error = DEVICE_OK;

	switch (m_command)
	{
	case BLK_Cmd_Status:
		if (ImageGetImageSize(pHDD->m_imagehandle) == 0)
			pHDD->m_error = DEVICE_IO_ERROR;
		LOG_DISK("ST-BLK: %02X\n", pHDD->m_error);
		break;
	case SP_Cmd_busyStatus:
		LOG_DISK("ST-BSY: %02X\n", pHDD->m_error);
		break;
	case SP_Cmd_status:
		pHDD->m_error = SmartPortCmdStatus(pHDD, nExecutedCycles);
		LOG_DISK("ST: %02X (statusCode: %02X)\n", pHDD->m_error, m_statusCode);
		break;
	case BLK_Cmd_Read:
	case SP_Cmd_readblock:
		LOG_DISK("RD: %08X (to addr: %04X)\n", pHDD->m_diskblock, pHDD->m_memblock);
		pHDD->m_status_next = DISK_STATUS_READ;
		if ((pHDD->m_diskblock * HD_BLOCK_SIZE) < ImageGetImageSize(pHDD->m_imagehandle))
		{
			bool breakpointHit = false;

			bool bRes = ImageReadBlock(pHDD->m_imagehandle, pHDD->m_diskblock, pHDD->m_buf);
			if (bRes)
			{
				pHDD->m_buf_ptr = 0;

				// Apple II's MMU could be setup so that read & write memory is different,
				// so can't use 'mem' (like we can for HDD block writes)
				WORD dstAddr = pHDD->m_memblock;
				UINT remaining = HD_BLOCK_SIZE;
				BYTE* pSrc = pHDD->m_buf;

				while (remaining)
				{
					memdirty[dstAddr >> 8] = 0xFF;
					LPBYTE page = memwrite[dstAddr >> 8];
					if (!page)	// I/O space or ROM
					{
						if (g_nAppMode == MODE_STEPPING)
							DebuggerBreakOnDmaToOrFromIoMemory(dstAddr, true);	//  GH#1007
						//else // Show MessageBox?

						bRes = false;
						break;
					}

					// handle both page-aligned & non-page aligned destinations
					UINT size = PAGE_SIZE - (dstAddr & 0xff);
					if (size > remaining) size = remaining;	// clip the last memcpy for the unaligned case

					if (g_nAppMode == MODE_STEPPING)
						breakpointHit = DebuggerCheckMemBreakpoints(dstAddr, size, true);	// GH#1103

					memcpy(page + (dstAddr & 0xff), pSrc, size);
					pSrc += size;
					dstAddr = (dstAddr + size) & (MEMORY_LENGTH - 1);	// wraps at 64KiB boundary

					remaining -= size;
				}
			}

			if (bRes)
			{
				pHDD->m_error = DEVICE_OK;

				if (!breakpointHit)
					m_notBusyCycle = g_nCumulativeCycles + (UINT64)CYCLES_FOR_DMA_RW_BLOCK;
#if DEBUG_SKIP_BUSY_STATUS
				m_notBusyCycle = 0;
#endif
			}
			else
			{
				pHDD->m_error = DEVICE_IO_ERROR;
			}
		}
		else
		{
			pHDD->m_error = DEVICE_IO_ERROR;
		}
		break;
	case BLK_Cmd_Write:
	case SP_Cmd_writeblock:
	{
		LOG_DISK("WR: %08X (from addr: %04X) %s\n", pHDD->m_diskblock, pHDD->m_memblock, pHDD->m_bWriteProtected ? "write-protected" : "");
		if (pHDD->m_bWriteProtected)
		{
			pHDD->m_status_next = DISK_STATUS_PROT;
			pHDD->m_error = NOWRITE;
		}
		else
		{
			pHDD->m_status_next = DISK_STATUS_WRITE;
			bool bRes = true;
			const bool bAppendBlocks = (pHDD->m_diskblock * HD_BLOCK_SIZE) >= ImageGetImageSize(pHDD->m_imagehandle);
			bool breakpointHit = false;

			if (bAppendBlocks)
			{
				memset(pHDD->m_buf, 0, HD_BLOCK_SIZE);

				// Inefficient (especially for gzip/zip files!)
				UINT uBlock = ImageGetImageSize(pHDD->m_imagehandle) / HD_BLOCK_SIZE;
				while (uBlock < pHDD->m_diskblock)
				{
					bRes = ImageWriteBlock(pHDD->m_imagehandle, uBlock++, pHDD->m_buf);
					_ASSERT(bRes);
					if (!bRes)
						break;
				}
			}

			// Trap and error on any accesses that overlap with I/O memory (GH#1007)
			if ((pHDD->m_memblock < APPLE_IO_BEGIN && ((pHDD->m_memblock + HD_BLOCK_SIZE - 1) >= APPLE_IO_BEGIN))	// 1) Starts before I/O, but ends in I/O memory
				|| ((pHDD->m_memblock >> 12) == (APPLE_IO_BEGIN >> 12)))											// 2) Starts in I/O memory
			{
				WORD dstAddr = ((pHDD->m_memblock >> 12) == (APPLE_IO_BEGIN >> 12)) ? pHDD->m_memblock : APPLE_IO_BEGIN;

				if (g_nAppMode == MODE_STEPPING)
					DebuggerBreakOnDmaToOrFromIoMemory(dstAddr, false);
				//else // Show MessageBox?

				bRes = false;
			}
			else
			{
				// NB. Do the writes in units of PAGE_SIZE so that DMA breakpoints are consistent with reads
				WORD srcAddr = pHDD->m_memblock;
				UINT remaining = HD_BLOCK_SIZE;
				BYTE* pDst = pHDD->m_buf;

				while (remaining)
				{
					UINT size = PAGE_SIZE - (srcAddr & 0xff);
					if (size > remaining) size = remaining;	// clip the last memcpy for the unaligned case

					if (g_nAppMode == MODE_STEPPING)
						breakpointHit = DebuggerCheckMemBreakpoints(srcAddr, size, false);

					memcpy(pDst, mem + srcAddr, size);
					pDst += size;
					srcAddr = (srcAddr + size) & (MEMORY_LENGTH - 1);	// wraps at 64KiB boundary

					remaining -= size;
				}
			}

			if (bRes)
				bRes = ImageWriteBlock(pHDD->m_imagehandle, pHDD->m_diskblock, pHDD->m_buf);

			if (bRes)
			{
				pHDD->m_error = DEVICE_OK;

				if (!breakpointHit)
					m_notBusyCycle = g_nCumulativeCycles + (UINT64)CYCLES_FOR_DMA_RW_BLOCK;
#if DEBUG_SKIP_BUSY_STATUS
				m_notBusyCycle = 0;
#endif
			}
			else
			{
				pHDD->m_error = DEVICE_IO_ERROR;
			}
		} // if (pHDD->m_bWriteProtected)
	}
	break;
	case BLK_Cmd_Format:
	case SP_Cmd_format:
		LOG_DISK("FORMAT: write-protected=%d\n", pHDD->m_bWriteProtected);
		if (pHDD->m_bWriteProtected)
		{
			pHDD->m_error = NOWRITE;
		}
		else
		{
			const UINT numBlocks = GetImageSizeInBlocks(pHDD->m_imagehandle);
			memset(pHDD->m_buf, 0, HD_BLOCK_SIZE);
			bool res = false;
			m_notBusyCycle = g_nCumulativeCycles;

			for (UINT block = 0; block < numBlocks; block++)
			{
				// Inefficient (especially for gzip/zip files!)
				res = ImageWriteBlock(pHDD->m_imagehandle, block, pHDD->m_buf);
				_ASSERT(res);
				if (!res)
					break;

				m_notBusyCycle += (UINT64)CYCLES_FOR_FORMATTING_1_BLOCK;
#if DEBUG_SKIP_BUSY_STATUS
				m_notBusyCycle = 0;
#endif
			}

			pHDD->m_error = res ? DEVICE_OK : DEVICE_IO_ERROR;
		}
		break;
	default:
		_ASSERT(0);
		pHDD->m_error = DEVICE_IO_ERROR;
		break;
	}

	return CmdStatus(pHDD);
}

BYTE HarddiskInterfaceCard::CmdStatus(HardDiskDrive* pHDD)
{
	BYTE r = 0;

	if (pHDD->m_error)
		r = STATUS_ERROR;		// Firmware requires that b0=1 for an error

	if (g_nCumulativeCycles <= m_notBusyCycle)
		r |= STATUS_BUSY;		// Firmware requires that b7=1 for busy (eg. busy doing r/w DMA operation)
	else
		pHDD->m_status_next = DISK_STATUS_OFF; // TODO: FIXME: ??? YELLOW ??? WARNING

	// Firmware requires that error code is [b6..1]
	_ASSERT(pHDD->m_error <= ERRORCODE_MASK);
	r |= (pHDD->m_error & ERRORCODE_MASK) << 1;

	return r;
}

//-----------------------------------------------------------------------------

BYTE __stdcall HarddiskInterfaceCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	HarddiskInterfaceCard* pCard = (HarddiskInterfaceCard*)MemGetSlotParameters(slot);

	HardDiskDrive* pHDD = NULL;
	BYTE addrIdx = addr & 0xF;
	if (addrIdx != 0x2 && addrIdx != 0x3	// GetUnit() depends on m_command & m_unitNum
		&& !((addrIdx == 0x9 || addrIdx == 0xA) && pCard->m_fifoIdx < 2))
	{
		pHDD = pCard->GetUnit();
		if (pHDD == NULL)
		{
			// Ensure that fifoIdx returns to 0, even if unitNum is out of range
			// EG. ProDOS 8 v2.5.0: Bitsy Bye: TAB through S7 volumes
			const UINT fifoSize = (addrIdx == 0x9) ? 6 : 7;
			pCard->m_fifoIdx = (pCard->m_fifoIdx + 1) % fifoSize;
			return SET_STATUS_ERROR(DEVICE_NOT_CONNECTED);
		}
	}

	if (addrIdx == 0x9 || addrIdx == 0xA)	// BLK or SP cmd FIFO
	{
		if (addrIdx == 0xA && pCard->m_fifoIdx == 0)
				d |= SP_Cmd_base;

		const UINT fifoSize = (addrIdx == 0x9) ? 6 : 7;
		addrIdx = 0x2 + pCard->m_fifoIdx;
		pCard->m_fifoIdx = (pCard->m_fifoIdx + 1) % fifoSize;
	}

	switch (addrIdx)
	{
	case 0x0:	// r/o: status
	case 0x1:	// r/o: execute
		// Writing to these read-only registers is a no-op.
		// NB. Don't change m_status_next, as UpdateLightStatus() has a huge performance cost!
		// Some HDC's firmware has a busy-wait loop doing "rol hd_status,x"
		// - this RMW opcode does an IORead() then an IOWrite(), and the loop iterates ~100 times!
		break;
	case 0x2:
		pCard->m_command = d;
		break;
	case 0x3:
		// b7    = drive#
		// b6..4 = slot#
		// b3..0 = ?
		pCard->m_unitNum = d;
		pCard->FixupUnitNum();
		break;
	case 0x4:
		pHDD->m_memblock = (pHDD->m_memblock & 0xFF00) | d;
		break;
	case 0x5:
		pHDD->m_memblock = (pHDD->m_memblock & 0x00FF) | (d << 8);
		break;
	case 0x6:
		if (pCard->m_command != SP_Cmd_status)
			pHDD->m_diskblock = (pHDD->m_diskblock & 0xFFFF00) | d;
		else
			pCard->m_statusCode = d;
		break;
	case 0x7:
		pHDD->m_diskblock = (pHDD->m_diskblock & 0xFF00FF) | (d << 8);
		break;
	case 0x8:
		if (pCard->m_command & SP_Cmd_base)
			pHDD->m_diskblock = (pHDD->m_diskblock & 0x00FFFF) | (d << 16);
		break;
	default:
		pHDD->m_status_next = DISK_STATUS_OFF;
	}

	if (pHDD)
	{
		if ((pCard->m_command & SP_Cmd_base) == 0)
			pHDD->m_diskblock &= 0x00FFFF;	// BLK cmds are only 16-bit

		pCard->UpdateLightStatus(pHDD);
	}

	return 0;
}

//===========================================================================

void HarddiskInterfaceCard::FixupUnitNum(void)
{
	if (!m_isFirmwareV1or2)
		return;

	// Older firmwares can write unitNum with 0x00
	if ((m_unitNum >> 4) != m_slot)
		m_unitNum = (m_unitNum & 0x8F) | (m_slot << 4);
}

BYTE HarddiskInterfaceCard::GetNumConnectedDevices(void)
{
	// Scan backwards to find the index of the last attached HDD
	int numDevices = NUM_HARDDISKS - 1;

	for (; numDevices >= 0; numDevices--)
	{
		if (m_hardDiskDrive[numDevices].m_imageloaded)
			break;
	}

	return numDevices + 1;
}

BYTE HarddiskInterfaceCard::GetProDOSBlockDeviceUnit(void)
{
	const BYTE slotFromUnitNum = (m_unitNum >> 4) & 7;
	const BYTE offset = (slotFromUnitNum == m_slot) ? 0 : 2;
	return offset + (m_unitNum >> 7);	// bit7 = drive select
}

HardDiskDrive* HarddiskInterfaceCard::GetUnit(void)
{
	const bool isSmartPortCmd = m_command & SP_Cmd_base;

	if (!isSmartPortCmd)
		return &m_hardDiskDrive[GetProDOSBlockDeviceUnit()];

	if (m_unitNum > kMaxSmartPortUnits)
		return NULL;

	if (m_unitNum > GetNumConnectedDevices())
		return NULL;

	if (m_unitNum == 0)
		return &m_smartPortController;

	return &m_hardDiskDrive[m_unitNum - 1];
}

void HarddiskInterfaceCard::SetIdString(std::vector<BYTE>& status, const std::string& idStr)
{
	const BYTE kMaxIdStrLen = 16;
	size_t idStrLen = idStr.length();
	if (idStrLen > kMaxIdStrLen)
		idStrLen = kMaxIdStrLen;
	status.push_back((BYTE)idStrLen);	// Set 'ID string length'

	for (UINT i = 0; i < idStrLen; i++)
		status.push_back(idStr[i]);

	for (UINT i = idStrLen; i < kMaxIdStrLen; i++)
		status.push_back(' ');	// ID string padded with ASCII spaces
}

BYTE HarddiskInterfaceCard::SmartPortCmdStatus(HardDiskDrive* pHDD, const ULONG nExecutedCycles)
{
	// Make Firmware version: eg. 1.30.18.0 => 130.18
	UINT fwVerMajorCheck = g_AppleWinVersion[0] * 100 + g_AppleWinVersion[1];
	_ASSERT(fwVerMajorCheck < 256);
	if (fwVerMajorCheck >= 256) fwVerMajorCheck = 255;
	UINT fwVerMinorCheck = g_AppleWinVersion[2];
	_ASSERT(fwVerMinorCheck < 256);
	if (fwVerMinorCheck >= 256) fwVerMinorCheck = 255;
	const BYTE fwVerMajor = fwVerMajorCheck;
	const BYTE fwVerMinor = fwVerMinorCheck;

	//

	BYTE r = DEVICE_OK;
	std::vector<BYTE> status;

	if (m_unitNum == 0)	// Unit-0: SmartPort Controller
	{
		BYTE numDevices = GetNumConnectedDevices();

		switch (m_statusCode)
		{
		case SP_Cmd_status_STATUS:
		case SP_Cmd_status_GETDIB:
		{
			// SmartPort driver status (8 bytes)
			status.push_back(numDevices);
			for (UINT i = 0; i < 7; i++)
				status.push_back(0);		// reserved
			if (m_statusCode == SP_Cmd_status_STATUS)
				break;
			// Device Information Block (DIB)
			std::string idStr = "AppleWin SP";
			SetIdString(status, idStr);
			status.push_back(0x00);			// device type (0x00: Apple II memory expansion card)
			status.push_back(0x00);			// device subtype (0x00: Apple II memory expansion card)
			status.push_back(fwVerMajor);	// f/w version (major)
			status.push_back(fwVerMinor);	// f/w version (minor)
			break;
		}
		case SP_Cmd_status_GETDCB:
		case SP_Cmd_status_GETNL:
		default:
			pHDD->m_error = 1;
			r = BADCTL;
			break;
		}
	}
	else	// Unit > 0: SmartPort Devices
	{
		switch (m_statusCode)
		{
		case SP_Cmd_status_STATUS:
		case SP_Cmd_status_GETDIB:
		{
			// Device status (4 bytes)
			const bool isImageLoaded = m_hardDiskDrive[m_unitNum - 1].m_imageloaded;

			// general status:
			// . b7=block device, b6=write allowed, b5=read allowed, b4=device online or disk in drive,
			// . b3=format allowed, b2=media write protected (block devices only), b1=device currently interrupting (//c only), b0=device currently open (char device only)
			BYTE generalStatus = isImageLoaded ? 0xF8 : 0xE8;			// Loaded: b#11111000: bwrlf--- / Not loaded: b#11101000: bwr-f---
			if (pHDD->m_bWriteProtected) generalStatus |= (1 << 2);
			status.push_back(generalStatus);

			const UINT imageSizeInBlocks = isImageLoaded ? GetImageSizeInBlocks(pHDD->m_imagehandle) : 0;
			status.push_back(imageSizeInBlocks & 0xff);			// num blocks (lo)
			status.push_back((imageSizeInBlocks >> 8) & 0xff);	// num blocks (med)
			status.push_back((imageSizeInBlocks >> 16) & 0xff);	// num blocks (hi)

			if (m_statusCode == SP_Cmd_status_STATUS)
				break;

			// Device Information Block (DIB)
			std::string idStr = "AppleWin SP D#";	// + "01".."99" (device number in decimal)
			idStr += (char)('0' + m_unitNum / 10);
			idStr += (char)('0' + m_unitNum % 10);
			SetIdString(status, idStr);
			status.push_back(0x02);			// device type (0x02: Hard disk)
			status.push_back(0x20);			// device subtype (0x20: Hard disk)
			status.push_back(fwVerMajor);	// f/w version (major)
			status.push_back(fwVerMinor);	// f/w version (minor)
			break;
		}
		case SP_Cmd_status_GETDCB:
		case SP_Cmd_status_GETNL:
		default:
			pHDD->m_error = 1;
			r = BADCTL;
			break;
		}
	}

	if (r == DEVICE_OK)
	{
		// Apple II's MMU could be setup so that read & write memory is different,
		// so can't use 'mem' directly, instead use CpuWrite(). (GH#1319)
		WORD statusListAddr = pHDD->m_memblock;

		// Check that writes don't hit I/O space or ROM
		BYTE page = statusListAddr >> 8;
		const BYTE endPage = (statusListAddr + (WORD)status.size()) >> 8;	// OK if endPage wraps to 0x00
		do
		{
			if (!memwrite[page])	// I/O space or ROM
			{
				if (g_nAppMode == MODE_STEPPING)
					DebuggerBreakOnDmaToOrFromIoMemory(page<<8, true);
				//else // Show MessageBox?

				pHDD->m_error = 1;
				r = BADCTL;
				break;
			}
		}
		while (page++ != endPage);

		if (r == DEVICE_OK)
		{
			for (BYTE i : status)
				CpuWrite(statusListAddr++, i, nExecutedCycles);
		}
	}

	return r;
}

//===========================================================================

UINT HarddiskInterfaceCard::GetImageSizeInBlocks(ImageInfo* const pImageInfo, const bool is16bit/*=false*/)
{
	if (m_userNumBlocks != 0)
		return m_userNumBlocks;
	UINT numberOfBlocks = (pImageInfo ? pImageInfo->uImageSize : 0) / HD_BLOCK_SIZE;
	if (numberOfBlocks > kHarddiskMaxNumBlocks)
		numberOfBlocks = kHarddiskMaxNumBlocks;
	if (is16bit && numberOfBlocks > 0xffff)
		numberOfBlocks = 0xffff;
	return numberOfBlocks;
}

//===========================================================================

void HarddiskInterfaceCard::UpdateLightStatus(HardDiskDrive* pHDD)
{
	if (pHDD->m_status_prev != pHDD->m_status_next) // Update LEDs if state changes
	{
		pHDD->m_status_prev = pHDD->m_status_next;
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);
	}
}

void HarddiskInterfaceCard::GetLightStatus(Disk_Status_e *pDisk1Status)
{
	const BYTE unit = (m_command & SP_Cmd_base) ? m_unitNum : GetProDOSBlockDeviceUnit();
	HardDiskDrive* pHDD = &m_hardDiskDrive[unit];
	*pDisk1Status = pHDD->m_status_prev;
}

//===========================================================================

bool HarddiskInterfaceCard::ImageSwap(void)
{
	std::swap(m_hardDiskDrive[HARDDISK_1], m_hardDiskDrive[HARDDISK_2]);

	SaveLastDiskImage(HARDDISK_1);
	SaveLastDiskImage(HARDDISK_2);

	GetFrame().FrameRefreshStatus(DRAW_LEDS);

	return true;
}

//===========================================================================

// Unit version history:
// 2: Updated $C7nn firmware to fix GH#319
// 3: Updated $Csnn firmware to fix GH#996 (now slot-independent code)
//    Added: Not Busy Cycle
// 4: Updated $Csnn firmware to fix GH#1264
// 5: Added: SP Status Code, FIFO Index & 256-byte firmware
//    Units are 1-based (up to v4 they were 0-based)
// 6: Added: absolute path
static const UINT kUNIT_VERSION = 6;

#define SS_YAML_VALUE_CARD_HDD "Generic HDD"

#define SS_YAML_KEY_CURRENT_UNIT "Current Unit"
#define SS_YAML_KEY_COMMAND "Command"

#define SS_YAML_KEY_HDDUNIT "Unit"
#define SS_YAML_KEY_FILENAME "Filename"
#define SS_YAML_KEY_ABSOLUTE_PATH "Absolute Path"
#define SS_YAML_KEY_ERROR "Error"
#define SS_YAML_KEY_MEMBLOCK "MemBlock"
#define SS_YAML_KEY_DISKBLOCK "DiskBlock"
#define SS_YAML_KEY_IMAGELOADED "ImageLoaded"
#define SS_YAML_KEY_STATUS_NEXT "Status Next"
#define SS_YAML_KEY_STATUS_PREV "Status Prev"
#define SS_YAML_KEY_BUF_PTR "Buffer Offset"
#define SS_YAML_KEY_BUF "Buffer"
#define SS_YAML_KEY_NOT_BUSY_CYCLE "Not Busy Cycle"
#define SS_YAML_KEY_SP_STATUS_CODE "SP Status Code"
#define SS_YAML_KEY_FIFO_INDEX "FIFO Index"
#define SS_YAML_KEY_FIRMWARE "Firmware"

const std::string& HarddiskInterfaceCard::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_HDD);
	return name;
}

void HarddiskInterfaceCard::SaveSnapshotHDDUnit(YamlSaveHelper& yamlSaveHelper, const UINT unit)
{
	const UINT baseUnitNum = 1;	// Unit0 is the SP Controller, so SP units start from 1

	YamlSaveHelper::Label label(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_HDDUNIT, baseUnitNum + unit);
	yamlSaveHelper.SaveString(SS_YAML_KEY_FILENAME, m_hardDiskDrive[unit].m_fullname);
	yamlSaveHelper.SaveString(SS_YAML_KEY_ABSOLUTE_PATH, ImageGetPathname(m_hardDiskDrive[unit].m_imagehandle));
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_ERROR, m_hardDiskDrive[unit].m_error);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_MEMBLOCK, m_hardDiskDrive[unit].m_memblock);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_DISKBLOCK, m_hardDiskDrive[unit].m_diskblock);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_IMAGELOADED, m_hardDiskDrive[unit].m_imageloaded);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_STATUS_NEXT, m_hardDiskDrive[unit].m_status_next);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_STATUS_PREV, m_hardDiskDrive[unit].m_status_prev);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_BUF_PTR, m_hardDiskDrive[unit].m_buf_ptr);

	// New label
	{
		YamlSaveHelper::Label buffer(yamlSaveHelper, "%s:\n", SS_YAML_KEY_BUF);
		yamlSaveHelper.SaveMemory(m_hardDiskDrive[unit].m_buf, HD_BLOCK_SIZE);
	}
}

void HarddiskInterfaceCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.Save("%s: %d # b7=unit for ProDOS BLK device\n", SS_YAML_KEY_CURRENT_UNIT, m_unitNum);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_COMMAND, m_command);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_NOT_BUSY_CYCLE, m_notBusyCycle);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SP_STATUS_CODE, m_statusCode);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_FIFO_INDEX, m_fifoIdx);

	// New label
	{
		YamlSaveHelper::Label buffer(yamlSaveHelper, "%s:\n", SS_YAML_KEY_FIRMWARE);
		yamlSaveHelper.SaveMemory(mem + APPLE_IO_BEGIN + m_slot * APPLE_SLOT_SIZE, APPLE_SLOT_SIZE);
	}

	for (UINT i = 0; i < NUM_HARDDISKS; i++)
	{
		if (m_hardDiskDrive[i].m_imageloaded)
			SaveSnapshotHDDUnit(yamlSaveHelper, i);
	}
}

bool HarddiskInterfaceCard::LoadSnapshotHDDUnit(YamlLoadHelper& yamlLoadHelper, const UINT unit, const UINT version)
{
	const UINT baseUnitNum = (version >= 5) ? 1 : 0;

	std::string hddUnitName = std::string(SS_YAML_KEY_HDDUNIT) + (char)('0' + baseUnitNum + unit);
	if (!yamlLoadHelper.GetSubMap(hddUnitName))
		return false;	// No HDD plugged in for this unit#

	m_hardDiskDrive[unit].m_fullname.clear();
	m_hardDiskDrive[unit].m_imagename.clear();
	m_hardDiskDrive[unit].m_imageloaded = false;	// Default to false (until image is successfully loaded below)
	m_hardDiskDrive[unit].m_status_next = DISK_STATUS_OFF;
	m_hardDiskDrive[unit].m_status_prev = DISK_STATUS_OFF;

	const std::string simpleFilename = yamlLoadHelper.LoadString(SS_YAML_KEY_FILENAME);
	const std::string absolutePath = version >= 6 ? yamlLoadHelper.LoadString(SS_YAML_KEY_ABSOLUTE_PATH) : "";
	m_hardDiskDrive[unit].m_error = yamlLoadHelper.LoadUint(SS_YAML_KEY_ERROR);
	m_hardDiskDrive[unit].m_memblock = yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMBLOCK);
	m_hardDiskDrive[unit].m_diskblock = yamlLoadHelper.LoadUint(SS_YAML_KEY_DISKBLOCK);
	yamlLoadHelper.LoadBool(SS_YAML_KEY_IMAGELOADED);	// Consume
	Disk_Status_e diskStatusNext = (Disk_Status_e) yamlLoadHelper.LoadUint(SS_YAML_KEY_STATUS_NEXT);
	Disk_Status_e diskStatusPrev = (Disk_Status_e) yamlLoadHelper.LoadUint(SS_YAML_KEY_STATUS_PREV);
	m_hardDiskDrive[unit].m_buf_ptr = yamlLoadHelper.LoadUint(SS_YAML_KEY_BUF_PTR);
	if (m_hardDiskDrive[unit].m_buf_ptr >= sizeof(m_hardDiskDrive[unit].m_buf))	// pre-v3 save-states would leave m_buf_ptr==0x200 after reading a block
		m_hardDiskDrive[unit].m_buf_ptr = sizeof(m_hardDiskDrive[unit].m_buf) - 1;

	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_BUF))
		throw std::runtime_error(hddUnitName + ": Missing: " + SS_YAML_KEY_BUF);
	yamlLoadHelper.LoadMemory(m_hardDiskDrive[unit].m_buf, HD_BLOCK_SIZE);
	yamlLoadHelper.PopMap();

	yamlLoadHelper.PopMap();

	//

	bool userSelectedImageFolder = false;

	std::string filename = simpleFilename;
	if (!filename.empty())
	{
		DWORD dwAttributes = GetFileAttributes(filename.c_str());
		if (dwAttributes == INVALID_FILE_ATTRIBUTES && !absolutePath.empty())
		{
			// try the absolute path if present
			filename = absolutePath;
			dwAttributes = GetFileAttributes(filename.c_str());
		}

		if (dwAttributes == INVALID_FILE_ATTRIBUTES)
		{
			// ignore absolute name when opening the file dialog
			filename = simpleFilename;
			// Get user to browse for file
			userSelectedImageFolder = SelectImage(unit, filename.c_str());

			dwAttributes = GetFileAttributes(filename.c_str());
		}

		bool bImageError = (dwAttributes == INVALID_FILE_ATTRIBUTES);
		if (!bImageError)
		{
			if (!Insert(unit, filename.c_str()))
				bImageError = true;

			// Insert() sets up:
			// . m_imagename
			// . m_fullname
			// . m_imageloaded
			// . hd_status_next = DISK_STATUS_OFF
			// . hd_status_prev = DISK_STATUS_OFF

			m_hardDiskDrive[unit].m_status_next = diskStatusNext;
			m_hardDiskDrive[unit].m_status_prev = diskStatusPrev;
		}
	}

	return userSelectedImageFolder;
}

bool HarddiskInterfaceCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		ThrowErrorInvalidVersion(version);

	if (version <= 2 && (regs.pc >> 8) == (0xC0|m_slot))
		throw std::runtime_error("HDC card: 6502 is running old HDD firmware");

	m_unitNum = yamlLoadHelper.LoadUint(SS_YAML_KEY_CURRENT_UNIT);
	if (version < 5)
	{
		m_isFirmwareV1or2 = true;
		FixupUnitNum();
	}

	m_command = yamlLoadHelper.LoadUint(SS_YAML_KEY_COMMAND);

	if (version >= 3)
		m_notBusyCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_NOT_BUSY_CYCLE);

	m_saveStateFirmwareV1 = m_saveStateFirmwareV2 = false;
	if (version < 4)
		m_saveStateFirmwareV1 = true;
	else if (version == 4)
		m_saveStateFirmwareV2 = true;

	if (version >= 5)
	{
		m_statusCode = yamlLoadHelper.LoadUint(SS_YAML_KEY_SP_STATUS_CODE);
		m_fifoIdx = yamlLoadHelper.LoadUint(SS_YAML_KEY_FIFO_INDEX);

		//

		if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_FIRMWARE))
			throw std::runtime_error(std::string("HDC") + ": Missing: " + SS_YAML_KEY_FIRMWARE);
		yamlLoadHelper.LoadMemory(m_saveStateFirmware, APPLE_SLOT_SIZE);
		yamlLoadHelper.PopMap();
		m_saveStateFirmwareValid = true;

		// NB. A command line option can be used to ignore the HDC's firmware:
		// . Used by AppleWin-Test (regression suite) so that an older save-state file can be used to test newer HDC firmware (eg. GH#1207).
		if (Snapshot_GetIgnoreHdcFirmware())
			m_saveStateFirmwareValid = false;
	}

	// Unplug all HDDs first in case eg. HDD-2 is to be plugged in as HDD-1
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
	{
		Unplug(i);
		m_hardDiskDrive[i].clear();
	}

	bool userSelectedImageFolder = false;
	for (UINT i = 0; i < NUM_HARDDISKS; i++)
		userSelectedImageFolder |= LoadSnapshotHDDUnit(yamlLoadHelper, i, version);

	if (!userSelectedImageFolder)
		RegSaveString(REG_PREFS, REGVALUE_PREF_HDV_START_DIR, 1, Snapshot_GetPath());

	GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);

	return true;
}
