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
#include "CardManager.h"
#include "CPU.h"
#include "DiskImage.h"	// ImageError_e, Disk_Status_e
#include "Memory.h"
#include "Registry.h"
#include "SaveState.h"
#include "YamlHelper.h"

#include "Debugger/Debug.h"
#include "../resource/resource.h"

/*
Memory map (for slot 7):

    C0F0	(r)   EXECUTE AND RETURN STATUS
	C0F1	(r)   STATUS (or ERROR): b7=busy, b0=error
	C0F2	(r/w) COMMAND
	C0F3	(r/w) UNIT NUMBER
	C0F4	(r/w) LOW BYTE OF MEMORY BUFFER
	C0F5	(r/w) HIGH BYTE OF MEMORY BUFFER
	C0F6	(r/w) LOW BYTE OF BLOCK NUMBER
	C0F7	(r/w) HIGH BYTE OF BLOCK NUMBER
	C0F8    (r)   NEXT BYTE (legacy read-only port - still supported)

Firmware notes:
. ROR ABS16,X and ROL ABS16,X - only used for $C081+s*$10 STATUS register:
    6502:  double read (old data), write (old data), write (new data). The writes are harmless as writes to STATUS are ignored.
    65C02: double read (old data), write (new data). The write is harmless as writes to STATUS are ignored.
. STA ABS16,X does a false-read. This is harmless for writable I/O registers, since the false-read has no side effect.

*/

/*
Hard drive emulation in AppleWin.

Concept
    To emulate a 32mb hard drive connected to an Apple IIe via AppleWin.
    Designed to work with Autoboot Rom and Prodos.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDOS). To create
      an empty .HDV file, just create a 0 byte file.

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
          Ignored.  This would be used for low level formatting of the device
            (as in the case of a tape or SCSI drive, perhaps).

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
	Card(CT_GenericHDD, slot)
{
	if (m_slot != SLOT7)	// fixme
		ThrowErrorInvalidSlot();

	m_unitNum = HARDDISK_1 << 7;	// b7=unit

	// The HDD interface has a single Command register for both drives:
	// . ProDOS will write to Command before switching drives
	m_command = 0;

	// Interface busy doing DMA for r/w when current cycle is earlier than this cycle
	m_notBusyCycle = 0;

	m_saveDiskImage = true;	// Save the DiskImage name to Registry
}

HarddiskInterfaceCard::~HarddiskInterfaceCard(void)
{
	CleanupDriveInternal(HARDDISK_1);
	CleanupDriveInternal(HARDDISK_2);
}

void HarddiskInterfaceCard::Reset(const bool powerCycle)
{
	m_hardDiskDrive[HARDDISK_1].m_error = 0;
	m_hardDiskDrive[HARDDISK_2].m_error = 0;
}

//===========================================================================

void HarddiskInterfaceCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	const DWORD HARDDISK_FW_SIZE = APPLE_SLOT_SIZE;

	BYTE* pData = GetFrame().GetResource(IDR_HDDRVR_FW, "FIRMWARE", HARDDISK_FW_SIZE);
	if (pData == NULL)
		return;

	memcpy(pCxRomPeripheral + m_slot * APPLE_SLOT_SIZE, pData, HARDDISK_FW_SIZE);

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

void HarddiskInterfaceCard::NotifyInvalidImage(TCHAR* pszImageFilename)
{
	// TC: TO DO - see Disk2InterfaceCard::NotifyInvalidImage()

	std::string strText = StrFormat("Unable to open the file %s.",
									pszImageFilename);

	GetFrame().FrameMessageBox(strText.c_str(),
							   g_pAppTitle.c_str(),
							   MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}

//===========================================================================

void HarddiskInterfaceCard::LoadLastDiskImage(const int drive)
{
	_ASSERT(drive == HARDDISK_1 || drive == HARDDISK_2);

	const std::string regKey = (drive == HARDDISK_1)
		? REGVALUE_LAST_HARDDISK_1
		: REGVALUE_LAST_HARDDISK_2;

	char pathname[MAX_PATH];

	std::string regSection = RegGetConfigSlotSection(m_slot);
	if (RegLoadString(regSection.c_str(), regKey.c_str(), TRUE, pathname, MAX_PATH, TEXT("")) && (pathname[0] != 0))
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
	_ASSERT(drive == HARDDISK_1 || drive == HARDDISK_2);

	if (!m_saveDiskImage)
		return;

	std::string regSection = RegGetConfigSlotSection(m_slot);
	RegSaveValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, CT_GenericHDD);

	const std::string regKey = (drive == HARDDISK_1)
		? REGVALUE_LAST_HARDDISK_1
		: REGVALUE_LAST_HARDDISK_2;

	const std::string& pathName = HarddiskGetFullPathName(drive);

	RegSaveString(regSection.c_str(), regKey.c_str(), TRUE, pathName);

	//

	// For now, only update 'HDV Starting Directory' for slot7 & drive1
	// . otherwise you'll get inconsistent results if you set drive1, then drive2 (and the images were in different folders)
	if (m_slot != SLOT7 || drive != HARDDISK_1)
		return;

	TCHAR szPathName[MAX_PATH];
	StringCbCopy(szPathName, MAX_PATH, pathName.c_str());
	TCHAR* slash = _tcsrchr(szPathName, PATH_SEPARATOR);
	if (slash != NULL)
	{
		slash[1] = '\0';
		RegSaveString(REG_PREFS, REGVALUE_PREF_HDV_START_DIR, 1, szPathName);
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

	for (UINT i=HARDDISK_1; i<=HARDDISK_2; i++)
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
	m_saveDiskImage = false;
	CleanupDrive(HARDDISK_1);

	m_saveDiskImage = false;
	CleanupDrive(HARDDISK_2);

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
	TCHAR directory[MAX_PATH];
	TCHAR filename[MAX_PATH];

	StringCbCopy(filename, MAX_PATH, pszFilename);

	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, directory, MAX_PATH, TEXT(""));
	std::string title = StrFormat("Select HDV Image For HDD %d", drive + 1);

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = GetFrame().g_hFrameWindow;
	ofn.hInstance       = GetFrame().g_hInstance;
	ofn.lpstrFilter     = TEXT("Hard Disk Images (*.hdv,*.po,*.2mg,*.2img,*.gz,*.zip)\0*.hdv;*.po;*.2mg;*.2img;*.gz;*.zip\0")
						  TEXT("All Files\0*.*\0");
	ofn.lpstrFile       = filename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = directory;
	ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;	// Don't allow creation & hide the read-only checkbox
	ofn.lpstrTitle      = title.c_str();

	bool bRes = false;

	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			StringCbCat(filename, MAX_PATH, TEXT(".hdv"));
		
		if (Insert(drive, filename))
		{
			bRes = true;
		}
		else
		{
			NotifyInvalidImage(filename);
		}
	}

	return bRes;
}

bool HarddiskInterfaceCard::Select(const int iDrive)
{
	return SelectImage(iDrive, TEXT(""));
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

bool HarddiskInterfaceCard::IsDriveUnplugged(const int iDrive)
{
	return m_hardDiskDrive[iDrive].m_imageloaded == false;
}

//===========================================================================

#define DEVICE_OK				0x00
#define DEVICE_IO_ERROR			0x27
#define DEVICE_NOT_CONNECTED	0x28	// No device detected/connected

BYTE __stdcall HarddiskInterfaceCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	HarddiskInterfaceCard* pCard = (HarddiskInterfaceCard*)MemGetSlotParameters(slot);
	HardDiskDrive* pHDD = &(pCard->m_hardDiskDrive[pCard->m_unitNum >> 7]);	// bit7 = drive select

	CpuCalcCycles(nExecutedCycles);
	const UINT CYCLES_FOR_DMA_RW_BLOCK = HD_BLOCK_SIZE;
	const UINT PAGE_SIZE = 256;

	BYTE r = DEVICE_OK;
	pHDD->m_status_next = DISK_STATUS_READ;

	switch (addr & 0xF)
	{
		case 0x0:
			if (pHDD->m_imageloaded)
			{
				// based on loaded data block request, load block into memory
				// returns status
				switch (pCard->m_command)
				{
					default:
					case 0x00: //status
						if (ImageGetImageSize(pHDD->m_imagehandle) == 0)
						{
							pHDD->m_error = 1;
							r = DEVICE_IO_ERROR;
						}
						break;
					case 0x01: //read
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
									dstAddr = (dstAddr + size) & (MEMORY_LENGTH-1);	// wraps at 64KiB boundary

									remaining -= size;
								}
							}

							if (bRes)
							{
								pHDD->m_error = 0;
								r = 0;

								if (!breakpointHit)
									pCard->m_notBusyCycle = g_nCumulativeCycles + (UINT64)CYCLES_FOR_DMA_RW_BLOCK;
							}
							else
							{
								pHDD->m_error = 1;
								r = DEVICE_IO_ERROR;
							}
						}
						else
						{
							pHDD->m_error = 1;
							r = DEVICE_IO_ERROR;
						}
						break;
					case 0x02: //write
						{
							pHDD->m_status_next = DISK_STATUS_WRITE;	// or DISK_STATUS_PROT if we ever enable write-protect on HDD
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
								pHDD->m_error = 0;
								r = 0;

								if (!breakpointHit)
									pCard->m_notBusyCycle = g_nCumulativeCycles + (UINT64)CYCLES_FOR_DMA_RW_BLOCK;
							}
							else
							{
								pHDD->m_error = 1;
								r = DEVICE_IO_ERROR;
							}
						}
						break;
					case 0x03: //format
						pHDD->m_status_next = DISK_STATUS_WRITE;	// or DISK_STATUS_PROT if we ever enable write-protect on HDD
						break;
				}
			}
			else
			{
				pHDD->m_status_next = DISK_STATUS_OFF;
				pHDD->m_error = 1;
				r = DEVICE_NOT_CONNECTED;	// GH#452
			}
		break;
	case 0x1: // m_error
		if (pHDD->m_error & 0x7f)
			pHDD->m_error = 1;		// Firmware requires that b0=1 for an error
		else
			pHDD->m_error = 0;

		if (g_nCumulativeCycles <= pCard->m_notBusyCycle)
			pHDD->m_error |= 0x80;	// Firmware requires that b7=1 for busy (eg. busy doing r/w DMA operation)
		else
			pHDD->m_status_next = DISK_STATUS_OFF; // TODO: FIXME: ??? YELLOW ??? WARNING

		r = pHDD->m_error;
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
		r = (BYTE)(pHDD->m_memblock & 0xFF00 >> 8);
		break;
	case 0x6:
		r = (BYTE)(pHDD->m_diskblock & 0x00FF);
		break;
	case 0x7:
		r = (BYTE)(pHDD->m_diskblock & 0xFF00 >> 8);
		break;
	case 0x8:	// Legacy: continue to support this I/O port for old HDD firmware
		r = pHDD->m_buf[pHDD->m_buf_ptr];
		if (pHDD->m_buf_ptr < sizeof(pHDD->m_buf)-1)
			pHDD->m_buf_ptr++;
		break;
	default:
		pHDD->m_status_next = DISK_STATUS_OFF;
		r = IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	}

	pCard->UpdateLightStatus(pHDD);
	return r;
}

//-----------------------------------------------------------------------------

BYTE __stdcall HarddiskInterfaceCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	HarddiskInterfaceCard* pCard = (HarddiskInterfaceCard*)MemGetSlotParameters(slot);
	HardDiskDrive* pHDD = &(pCard->m_hardDiskDrive[pCard->m_unitNum >> 7]);	// bit7 = drive select

	BYTE r = DEVICE_OK;

	switch (addr & 0xF)
	{
	case 0x0:	// r/o: status
	case 0x1:	// r/o: execute
	case 0x8:	// r/o: legacy next-data port
		// Writing to these 3 read-only registers is a no-op.
		// NB. Don't change m_status_next, as UpdateLightStatus() has a huge performance cost!
		// Firmware has a busy-wait loop doing "rol hd_status,x"
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
		break;
	case 0x4:
		pHDD->m_memblock = (pHDD->m_memblock & 0xFF00) | d;
		break;
	case 0x5:
		pHDD->m_memblock = (pHDD->m_memblock & 0x00FF) | (d << 8);
		break;
	case 0x6:
		pHDD->m_diskblock = (pHDD->m_diskblock & 0xFF00) | d;
		break;
	case 0x7:
		pHDD->m_diskblock = (pHDD->m_diskblock & 0x00FF) | (d << 8);
		break;
	default:
		pHDD->m_status_next = DISK_STATUS_OFF;
		r = IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	}

	pCard->UpdateLightStatus(pHDD);
	return r;
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
	HardDiskDrive* pHDD = &m_hardDiskDrive[m_unitNum >> 7];	// bit7 = drive select
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
static const UINT kUNIT_VERSION = 3;

#define SS_YAML_VALUE_CARD_HDD "Generic HDD"

#define SS_YAML_KEY_CURRENT_UNIT "Current Unit"
#define SS_YAML_KEY_COMMAND "Command"

#define SS_YAML_KEY_HDDUNIT "Unit"
#define SS_YAML_KEY_FILENAME "Filename"
#define SS_YAML_KEY_ERROR "Error"
#define SS_YAML_KEY_MEMBLOCK "MemBlock"
#define SS_YAML_KEY_DISKBLOCK "DiskBlock"
#define SS_YAML_KEY_IMAGELOADED "ImageLoaded"
#define SS_YAML_KEY_STATUS_NEXT "Status Next"
#define SS_YAML_KEY_STATUS_PREV "Status Prev"
#define SS_YAML_KEY_BUF_PTR "Buffer Offset"
#define SS_YAML_KEY_BUF "Buffer"
#define SS_YAML_KEY_NOT_BUSY_CYCLE "Not Busy Cycle"

const std::string& HarddiskInterfaceCard::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_HDD);
	return name;
}

void HarddiskInterfaceCard::SaveSnapshotHDDUnit(YamlSaveHelper& yamlSaveHelper, UINT unit)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_HDDUNIT, unit);
	yamlSaveHelper.SaveString(SS_YAML_KEY_FILENAME, m_hardDiskDrive[unit].m_fullname);
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
	yamlSaveHelper.Save("%s: %d # b7=unit\n", SS_YAML_KEY_CURRENT_UNIT, m_unitNum);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_COMMAND, m_command);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_NOT_BUSY_CYCLE, m_notBusyCycle);

	SaveSnapshotHDDUnit(yamlSaveHelper, HARDDISK_1);
	SaveSnapshotHDDUnit(yamlSaveHelper, HARDDISK_2);
}

bool HarddiskInterfaceCard::LoadSnapshotHDDUnit(YamlLoadHelper& yamlLoadHelper, UINT unit)
{
	std::string hddUnitName = std::string(SS_YAML_KEY_HDDUNIT) + (unit == HARDDISK_1 ? std::string("0") : std::string("1"));
	if (!yamlLoadHelper.GetSubMap(hddUnitName))
		throw std::runtime_error("Card: Expected key: " + hddUnitName);

	m_hardDiskDrive[unit].m_fullname.clear();
	m_hardDiskDrive[unit].m_imagename.clear();
	m_hardDiskDrive[unit].m_imageloaded = false;	// Default to false (until image is successfully loaded below)
	m_hardDiskDrive[unit].m_status_next = DISK_STATUS_OFF;
	m_hardDiskDrive[unit].m_status_prev = DISK_STATUS_OFF;

	std::string filename = yamlLoadHelper.LoadString(SS_YAML_KEY_FILENAME);
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

	bool bResSelectImage = false;

	if (!filename.empty())
	{
		DWORD dwAttributes = GetFileAttributes(filename.c_str());
		if (dwAttributes == INVALID_FILE_ATTRIBUTES)
		{
			// Get user to browse for file
			bResSelectImage = SelectImage(unit, filename.c_str());

			dwAttributes = GetFileAttributes(filename.c_str());
		}

		bool bImageError = (dwAttributes == INVALID_FILE_ATTRIBUTES);
		if (!bImageError)
		{
			if (!Insert(unit, filename.c_str()))
				bImageError = true;

			// HD_Insert() sets up:
			// . m_imagename
			// . m_fullname
			// . m_imageloaded
			// . hd_status_next = DISK_STATUS_OFF
			// . hd_status_prev = DISK_STATUS_OFF

			m_hardDiskDrive[unit].m_status_next = diskStatusNext;
			m_hardDiskDrive[unit].m_status_prev = diskStatusPrev;
		}
	}

	return bResSelectImage;
}

bool HarddiskInterfaceCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		ThrowErrorInvalidVersion(version);

	if (version <= 2 && (regs.pc >> 8) == (0xC0|m_slot))
		throw std::runtime_error("HDD card: 6502 is running old HDD firmware");

	m_unitNum = yamlLoadHelper.LoadUint(SS_YAML_KEY_CURRENT_UNIT);	// b7=unit
	m_command = yamlLoadHelper.LoadUint(SS_YAML_KEY_COMMAND);

	if (version >= 3)
		m_notBusyCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_NOT_BUSY_CYCLE);

	// Unplug all HDDs first in case HDD-2 is to be plugged in as HDD-1
	for (UINT i=0; i<NUM_HARDDISKS; i++)
	{
		Unplug(i);
		m_hardDiskDrive[i].clear();
	}

	bool bResSelectImage1 = LoadSnapshotHDDUnit(yamlLoadHelper, HARDDISK_1);
	bool bResSelectImage2 = LoadSnapshotHDDUnit(yamlLoadHelper, HARDDISK_2);

	if (!bResSelectImage1 && !bResSelectImage2)
		RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, Snapshot_GetPath());

	GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);

	return true;
}
