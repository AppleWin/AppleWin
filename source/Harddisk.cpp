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
#include "DiskImageHelper.h"
#include "Memory.h"
#include "Registry.h"
#include "SaveState.h"
#include "YamlHelper.h"

#include "../resource/resource.h"

/*
Memory map:

    C0F0	(r)   EXECUTE AND RETURN STATUS
	C0F1	(r)   STATUS (or ERROR)
	C0F2	(r/w) COMMAND
	C0F3	(r/w) UNIT NUMBER
	C0F4	(r/w) LOW BYTE OF MEMORY BUFFER
	C0F5	(r/w) HIGH BYTE OF MEMORY BUFFER
	C0F6	(r/w) LOW BYTE OF BLOCK NUMBER
	C0F7	(r/w) HIGH BYTE OF BLOCK NUMBER
	C0F8    (r)   NEXT BYTE
*/

/*
Hard drive emulation in Applewin.

Concept
    To emulate a 32mb hard drive connected to an Apple IIe via Applewin.
    Designed to work with Autoboot Rom and Prodos.

Overview
  1. Hard drive image file
      The hard drive image file (.HDV) will be formatted into blocks of 512
      bytes, in a linear fashion. The internal formatting and meaning of each
      block to be decided by the Apple's operating system (ProDos). To create
      an empty .HDV file, just create a 0 byte file (I prefer the debug method).
  
  2. Emulation code
      There are 4 commands Prodos will send to a block device.
      Listed below are each command and how it's handled:

      1. STATUS
          In the emulation's case, returns only a DEVICE OK (0) or DEVICE I/O ERROR (8).
          DEVICE I/O ERROR only returned if no HDV file is selected.

      2. READ
          Loads requested block into a 512 byte buffer by attempting to seek to
            location in HDV file.
          If seek fails, returns a DEVICE I/O ERROR.  Resets hd_buf_ptr used by HD_NEXTBYTE
          Returns a DEVICE OK if read was successful, or a DEVICE I/O ERROR otherwise.

      3. WRITE
          Copies requested block from the Apple's memory to a 512 byte buffer
            then attempts to seek to requested block.
          If the seek fails (usually because the seek is beyond the EOF for the
            HDV file), the Emulation will attempt to "grow" the HDV file to accomodate.
            Once the file can accomodate, or if the seek did not fail, the buffer is
            written to the HDV file.  NOTE: A2PC will grow *AND* shrink the HDV file.
          I didn't see the point in shrinking the file as this behaviour would require
            patching prodos (to detect DELETE FILE calls).

      4. FORMAT
          Ignored.  This would be used for low level formatting of the device
            (as in the case of a tape or SCSI drive, perhaps).

  3. Bugs
      The only thing I've noticed is that Copy II+ 7.1 seems to crash or stall
      occasionally when trying to calculate how many free block are available
      when running a catalog.  This might be due to the great number of blocks
      available.  Also, DDD pro will not optimise the disk correctally (it's
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

struct HDD
{
	HDD()
	{
		clear();
	}

	void clear()
	{
		// This is not a POD (there is a std::string)
		// memset(0) does not work
		imagename.clear();
		fullname.clear();
		strFilenameInZip.clear();
		imagehandle = NULL;
		bWriteProtected = false;
		hd_error = 0;
		hd_memblock = 0;
		hd_diskblock = 0;
		hd_buf_ptr = 0;
		hd_imageloaded = false;
		memset(hd_buf, 0, sizeof(hd_buf));
#if HD_LED
		hd_status_next = DISK_STATUS_OFF;
		hd_status_prev = DISK_STATUS_OFF;
#endif
	}

	// From FloppyDisk
	std::string	imagename;	// <FILENAME> (ie. no extension)
	std::string fullname;	// <FILENAME.EXT> or <FILENAME.zip>
	std::string strFilenameInZip;					// ""             or <FILENAME.EXT> [not used]
	ImageInfo*	imagehandle;			// Init'd by HD_Insert() -> ImageOpen()
	bool	bWriteProtected;			// Needed for ImageOpen() [otherwise not used]
	//
	BYTE	hd_error;		// NB. Firmware requires that b0=0 (OK) or b0=1 (Error)
	WORD	hd_memblock;
	UINT	hd_diskblock;
	WORD	hd_buf_ptr;
	bool	hd_imageloaded;
	BYTE	hd_buf[HD_BLOCK_SIZE+1];	// Why +1? Probably for erroreous reads beyond the block size (ie. reads from I/O addr 0xC0F8)

#if HD_LED
	Disk_Status_e hd_status_next;
	Disk_Status_e hd_status_prev;
#endif
};

static bool	g_bHD_RomLoaded = false;
static bool g_bHD_Enabled = false;

static BYTE	g_nHD_UnitNum = HARDDISK_1<<7;	// b7=unit

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static BYTE	g_nHD_Command;

static HDD g_HardDisk[NUM_HARDDISKS];

static bool g_bSaveDiskImage = true;	// Save the DiskImage name to Registry
static UINT g_uSlot = 7;

//===========================================================================

static void HD_SaveLastDiskImage(const int iDrive);

static void HD_CleanupDrive(const int iDrive)
{
	if (g_HardDisk[iDrive].imagehandle)
	{
		ImageClose(g_HardDisk[iDrive].imagehandle);
		g_HardDisk[iDrive].imagehandle = NULL;
	}

	g_HardDisk[iDrive].hd_imageloaded = false;

	g_HardDisk[iDrive].imagename.clear();
	g_HardDisk[iDrive].fullname.clear();
	g_HardDisk[iDrive].strFilenameInZip.clear();

	HD_SaveLastDiskImage(iDrive);
}

//-----------------------------------------------------------------------------

static void NotifyInvalidImage(TCHAR* pszImageFilename)
{
	// TC: TO DO
}

//===========================================================================

BOOL HD_Insert(const int iDrive, const std::string & pszImageFilename);

void HD_LoadLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == HARDDISK_1 || iDrive == HARDDISK_2);

	const char *pRegKey = (iDrive == HARDDISK_1)
		? REGVALUE_PREF_LAST_HARDDISK_1
		: REGVALUE_PREF_LAST_HARDDISK_2;

	TCHAR sFilePath[MAX_PATH];
	if (RegLoadString(TEXT(REG_PREFS), pRegKey, 1, sFilePath, MAX_PATH, TEXT("")))
	{
		g_bSaveDiskImage = false;
		// Pass in ptr to local copy of filepath, since RemoveDisk() sets DiskPathFilename = ""		// todo: update comment for HD func
		HD_Insert(iDrive, sFilePath);
		g_bSaveDiskImage = true;
	}
}

//===========================================================================

static void HD_SaveLastDiskImage(const int iDrive)
{
	_ASSERT(iDrive == HARDDISK_1 || iDrive == HARDDISK_2);

	if (!g_bSaveDiskImage)
		return;

	const std::string & pFileName = g_HardDisk[iDrive].fullname;

	if (iDrive == HARDDISK_1)
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_HARDDISK_1, TRUE, pFileName);
	else
		RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_LAST_HARDDISK_2, TRUE, pFileName);

	//

	char szPathName[MAX_PATH];
	strcpy(szPathName, HD_GetFullPathName(iDrive).c_str());
	if (_tcsrchr(szPathName, TEXT('\\')))
	{
		char* pPathEnd = _tcsrchr(szPathName, TEXT('\\'))+1;
		*pPathEnd = 0;
		RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, szPathName);
	}
}

//===========================================================================

// (Nearly) everything below is global

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

static const DWORD HDDRVR_SIZE = APPLE_SLOT_SIZE;

bool HD_CardIsEnabled(void)
{
	return g_bHD_RomLoaded && g_bHD_Enabled;
}

// Called by:
// . LoadConfiguration() - Done at each restart
// . RestoreCurrentConfig() - Done when Config dialog is cancelled
// . Snapshot_LoadState_v2() - Done to default to disabled state
void HD_SetEnabled(const bool bEnabled)
{
	if(g_bHD_Enabled == bEnabled)
		return;

	g_bHD_Enabled = bEnabled;

	if (bEnabled)
		GetCardMgr().Insert(SLOT7, CT_GenericHDD);
	else
		GetCardMgr().Remove(SLOT7);

#if 0
	// FIXME: For LoadConfiguration(), g_uSlot=7 (see definition at start of file)
	// . g_uSlot is only really setup by HD_Load_Rom(), later on
	RegisterIoHandler(g_uSlot, HD_IO_EMUL, HD_IO_EMUL, NULL, NULL, NULL, NULL);

	LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
	if(pCxRomPeripheral == NULL)	// This will be NULL when called after loading value from Registry
		return;

	//

	if(g_bHD_Enabled)
		HD_Load_Rom(pCxRomPeripheral, g_uSlot);
	else
		memset(pCxRomPeripheral + g_uSlot*256, 0, HDDRVR_SIZE);
#endif
}

//-------------------------------------

const std::string & HD_GetFullName(const int iDrive)
{
	return g_HardDisk[iDrive].fullname;
}

const std::string & HD_GetFullPathName(const int iDrive)
{
	return ImageGetPathname(g_HardDisk[iDrive].imagehandle);
}

static const std::string & HD_DiskGetBaseName(const int iDrive)
{
	return g_HardDisk[iDrive].imagename;
}

void HD_GetFilenameAndPathForSaveState(std::string& filename, std::string& path)
{
	filename = "";
	path = "";

	if (!g_bHD_Enabled)
		return;

	for (UINT i=HARDDISK_1; i<=HARDDISK_2; i++)
	{
		if (!g_HardDisk[i].hd_imageloaded)
			continue;

		filename = HD_DiskGetBaseName(i);
		std::string pathname = HD_GetFullPathName(i);

		int idx = pathname.find_last_of('\\');
		if (idx >= 0 && idx+1 < (int)pathname.length())	// path exists?
		{
			path = pathname.substr(0, idx+1);
			return;
		}

		_ASSERT(0);
		break;
	}
}

//-------------------------------------

void HD_Reset(void)
{
	g_HardDisk[HARDDISK_1].hd_error = 0;
	g_HardDisk[HARDDISK_2].hd_error = 0;
}

//-------------------------------------

void HD_Load_Rom(const LPBYTE pCxRomPeripheral, const UINT uSlot)
{
	if(!g_bHD_Enabled)
		return;

	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_HDDRVR_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != HDDRVR_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	g_uSlot = uSlot;
	memcpy(pCxRomPeripheral + uSlot*256, pData, HDDRVR_SIZE);
	g_bHD_RomLoaded = true;

	RegisterIoHandler(g_uSlot, HD_IO_EMUL, HD_IO_EMUL, NULL, NULL, NULL, NULL);
}

void HD_Destroy(void)
{
	g_bSaveDiskImage = false;
	HD_CleanupDrive(HARDDISK_1);

	g_bSaveDiskImage = false;
	HD_CleanupDrive(HARDDISK_2);

	g_bSaveDiskImage = true;
}

// Pre: pszImageFilename is qualified with path
BOOL HD_Insert(const int iDrive, const std::string & pszImageFilename)
{
	if (pszImageFilename.empty())
		return FALSE;

	if (g_HardDisk[iDrive].hd_imageloaded)
		HD_Unplug(iDrive);

	// Check if image is being used by the other HDD, and unplug it in order to be swapped
	{
		const std::string & pszOtherPathname = HD_GetFullPathName(!iDrive);

		char szCurrentPathname[MAX_PATH]; 
		DWORD uNameLen = GetFullPathName(pszImageFilename.c_str(), MAX_PATH, szCurrentPathname, NULL);
		if (uNameLen == 0 || uNameLen >= MAX_PATH)
			strcpy_s(szCurrentPathname, MAX_PATH, pszImageFilename.c_str());

		if (!strcmp(pszOtherPathname.c_str(), szCurrentPathname))
		{
			HD_Unplug(!iDrive);
			GetFrame().FrameRefreshStatus(DRAW_LEDS);
		}
	}

	const bool bCreateIfNecessary = false;		// NB. Don't allow creation of HDV files
	const bool bExpectFloppy = false;
	const bool bIsHarddisk = true;
	ImageError_e Error = ImageOpen(pszImageFilename,
		&g_HardDisk[iDrive].imagehandle,
		&g_HardDisk[iDrive].bWriteProtected,
		bCreateIfNecessary,
		g_HardDisk[iDrive].strFilenameInZip,	// TODO: Use this
		bExpectFloppy);

	g_HardDisk[iDrive].hd_imageloaded = (Error == eIMAGE_ERROR_NONE);

#if HD_LED
	g_HardDisk[iDrive].hd_status_next = DISK_STATUS_OFF;
	g_HardDisk[iDrive].hd_status_prev = DISK_STATUS_OFF;
#endif

	if (Error == eIMAGE_ERROR_NONE)
	{
		GetImageTitle(pszImageFilename.c_str(), g_HardDisk[iDrive].imagename, g_HardDisk[iDrive].fullname);
		Snapshot_UpdatePath();
	}

	HD_SaveLastDiskImage(iDrive);

	return g_HardDisk[iDrive].hd_imageloaded;
}

static bool HD_SelectImage(const int drive, LPCSTR pszFilename)
{
	TCHAR directory[MAX_PATH];
	TCHAR filename[MAX_PATH];
	TCHAR title[40];

	StringCbCopy(filename, MAX_PATH, pszFilename);

	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, directory, MAX_PATH, TEXT(""));
	StringCbPrintf(title, 40, TEXT("Select HDV Image For HDD %d"), drive + 1);

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
	ofn.lpstrTitle      = title;

	bool bRes = false;

	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			StringCbCat(filename, MAX_PATH, TEXT(".hdv"));
		
		if (HD_Insert(drive, filename))
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

bool HD_Select(const int iDrive)
{
	return HD_SelectImage(iDrive, TEXT(""));
}

void HD_Unplug(const int iDrive)
{
	if (g_HardDisk[iDrive].hd_imageloaded)
	{
		HD_CleanupDrive(iDrive);
		Snapshot_UpdatePath();
	}
}

bool HD_IsDriveUnplugged(const int iDrive)
{
	return g_HardDisk[iDrive].hd_imageloaded == false;
}

//-----------------------------------------------------------------------------

#define DEVICE_OK				0x00
#define DEVICE_UNKNOWN_ERROR	0x28
#define DEVICE_IO_ERROR			0x27

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	BYTE r = DEVICE_OK;
	addr &= 0xFF;

	if (!HD_CardIsEnabled())
		return r;

	HDD* pHDD = &g_HardDisk[g_nHD_UnitNum >> 7];	// bit7 = drive select
	
	if (bWrite == 0) // read
	{
#if HD_LED
		pHDD->hd_status_next = DISK_STATUS_READ;
#endif
		switch (addr)
		{
			case 0xF0:
				if (pHDD->hd_imageloaded)
				{
					// based on loaded data block request, load block into memory
					// returns status
					switch (g_nHD_Command)
					{
						default:
						case 0x00: //status
							if (ImageGetImageSize(pHDD->imagehandle) == 0)
							{
								pHDD->hd_error = 1;
								r = DEVICE_IO_ERROR;
							}
							break;
						case 0x01: //read
							if ((pHDD->hd_diskblock * HD_BLOCK_SIZE) < ImageGetImageSize(pHDD->imagehandle))
							{
								bool bRes = ImageReadBlock(pHDD->imagehandle, pHDD->hd_diskblock, pHDD->hd_buf);
								if (bRes)
								{
									pHDD->hd_error = 0;
									r = 0;
									pHDD->hd_buf_ptr = 0;
								}
								else
								{
									pHDD->hd_error = 1;
									r = DEVICE_IO_ERROR;
								}
							}
							else
							{
								pHDD->hd_error = 1;
								r = DEVICE_IO_ERROR;
							}
							break;
						case 0x02: //write
							{
#if HD_LED
								pHDD->hd_status_next = DISK_STATUS_WRITE;
#endif
								bool bRes = true;
								const bool bAppendBlocks = (pHDD->hd_diskblock * HD_BLOCK_SIZE) >= ImageGetImageSize(pHDD->imagehandle);

								if (bAppendBlocks)
								{
									memset(pHDD->hd_buf, 0, HD_BLOCK_SIZE);

									// Inefficient (especially for gzip/zip files!)
									UINT uBlock = ImageGetImageSize(pHDD->imagehandle) / HD_BLOCK_SIZE;
									while (uBlock < pHDD->hd_diskblock)
									{
										bRes = ImageWriteBlock(pHDD->imagehandle, uBlock++, pHDD->hd_buf);
										_ASSERT(bRes);
										if (!bRes)
											break;
									}
								}

								memmove(pHDD->hd_buf, mem+pHDD->hd_memblock, HD_BLOCK_SIZE);

								if (bRes)
									bRes = ImageWriteBlock(pHDD->imagehandle, pHDD->hd_diskblock, pHDD->hd_buf);

								if (bRes)
								{
									pHDD->hd_error = 0;
									r = 0;
								}
								else
								{
									pHDD->hd_error = 1;
									r = DEVICE_IO_ERROR;
								}
							}
							break;
						case 0x03: //format
#if HD_LED
							pHDD->hd_status_next = DISK_STATUS_WRITE;
#endif
							break;
					}
				}
				else
				{
#if HD_LED
					pHDD->hd_status_next = DISK_STATUS_OFF;
#endif
					pHDD->hd_error = 1;
					r = DEVICE_UNKNOWN_ERROR;
				}
			break;
		case 0xF1: // hd_error
#if HD_LED
			pHDD->hd_status_next = DISK_STATUS_OFF; // TODO: FIXME: ??? YELLOW ??? WARNING
#endif
			if (pHDD->hd_error)
			{
				_ASSERT(pHDD->hd_error & 1);
				pHDD->hd_error |= 1;	// Firmware requires that b0=1 for an error
			}

			r = pHDD->hd_error;
			break;
		case 0xF2:
			r = g_nHD_Command;
			break;
		case 0xF3:
			r = g_nHD_UnitNum;
			break;
		case 0xF4:
			r = (BYTE)(pHDD->hd_memblock & 0x00FF);
			break;
		case 0xF5:
			r = (BYTE)(pHDD->hd_memblock & 0xFF00 >> 8);
			break;
		case 0xF6:
			r = (BYTE)(pHDD->hd_diskblock & 0x00FF);
			break;
		case 0xF7:
			r = (BYTE)(pHDD->hd_diskblock & 0xFF00 >> 8);
			break;
		case 0xF8:
			r = pHDD->hd_buf[pHDD->hd_buf_ptr];
			if (pHDD->hd_buf_ptr < sizeof(pHDD->hd_buf)-1)
				pHDD->hd_buf_ptr++;
			break;
		default:
#if HD_LED
			pHDD->hd_status_next = DISK_STATUS_OFF;
#endif
			return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
		}
	}
	else // write to registers
	{
#if HD_LED
		pHDD->hd_status_next = DISK_STATUS_PROT; // TODO: FIXME: If we ever enable write-protect on HD then need to change to something else ...
#endif
		switch (addr)
		{
		case 0xF2:
			g_nHD_Command = d;
			break;
		case 0xF3:
			// b7    = drive#
			// b6..4 = slot#
			// b3..0 = ?
			g_nHD_UnitNum = d;
			break;
		case 0xF4:
			pHDD->hd_memblock = (pHDD->hd_memblock & 0xFF00) | d;
			break;
		case 0xF5:
			pHDD->hd_memblock = (pHDD->hd_memblock & 0x00FF) | (d << 8);
			break;
		case 0xF6:
			pHDD->hd_diskblock = (pHDD->hd_diskblock & 0xFF00) | d;
			break;
		case 0xF7:
			pHDD->hd_diskblock = (pHDD->hd_diskblock & 0x00FF) | (d << 8);
			break;
		default:
#if HD_LED
			pHDD->hd_status_next = DISK_STATUS_OFF;
#endif
			return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
		}
	}

#if HD_LED
	// 1.19.0.0 Hard Disk Status/Indicator Light
	if( pHDD->hd_status_prev != pHDD->hd_status_next ) // Update LEDs if state changes
	{
		pHDD->hd_status_prev = pHDD->hd_status_next;
		GetFrame().FrameRefreshStatus(DRAW_LEDS);
	}
#endif

	return r;
}

// 1.19.0.0 Hard Disk Status/Indicator Light
void HD_GetLightStatus (Disk_Status_e *pDisk1Status_)
{
#if HD_LED
	if ( HD_CardIsEnabled() )
	{
		HDD* pHDD = &g_HardDisk[g_nHD_UnitNum >> 7];	// bit7 = drive select
		*pDisk1Status_ = pHDD->hd_status_prev;
	} else
#endif
	{
		*pDisk1Status_ = DISK_STATUS_OFF;
	}
}

bool HD_ImageSwap(void)
{
	std::swap(g_HardDisk[HARDDISK_1], g_HardDisk[HARDDISK_2]);

	HD_SaveLastDiskImage(HARDDISK_1);
	HD_SaveLastDiskImage(HARDDISK_2);

	GetFrame().FrameRefreshStatus(DRAW_LEDS, false);

	return true;
}

//===========================================================================

// Unit version history:
// 2: Updated $Csnn firmware to fix GH#319
static const UINT kUNIT_VERSION = 2;

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

std::string HD_GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_HDD);
	return name;
}

static void HD_SaveSnapshotHDDUnit(YamlSaveHelper& yamlSaveHelper, UINT unit)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s%d:\n", SS_YAML_KEY_HDDUNIT, unit);
	yamlSaveHelper.SaveString(SS_YAML_KEY_FILENAME, g_HardDisk[unit].fullname);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_ERROR, g_HardDisk[unit].hd_error);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_MEMBLOCK, g_HardDisk[unit].hd_memblock);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_DISKBLOCK, g_HardDisk[unit].hd_diskblock);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_IMAGELOADED, g_HardDisk[unit].hd_imageloaded);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_STATUS_NEXT, g_HardDisk[unit].hd_status_next);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_STATUS_PREV, g_HardDisk[unit].hd_status_prev);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_BUF_PTR, g_HardDisk[unit].hd_buf_ptr);

	// New label
	{
		YamlSaveHelper::Label buffer(yamlSaveHelper, "%s:\n", SS_YAML_KEY_BUF);
		yamlSaveHelper.SaveMemory(g_HardDisk[unit].hd_buf, HD_BLOCK_SIZE);
	}
}

void HD_SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (!HD_CardIsEnabled())
		return;

	YamlSaveHelper::Slot slot(yamlSaveHelper, HD_GetSnapshotCardName(), g_uSlot, kUNIT_VERSION);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.Save("%s: %d # b7=unit\n", SS_YAML_KEY_CURRENT_UNIT, g_nHD_UnitNum);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_COMMAND, g_nHD_Command);

	HD_SaveSnapshotHDDUnit(yamlSaveHelper, HARDDISK_1);
	HD_SaveSnapshotHDDUnit(yamlSaveHelper, HARDDISK_2);
}

static bool HD_LoadSnapshotHDDUnit(YamlLoadHelper& yamlLoadHelper, UINT unit)
{
	std::string hddUnitName = std::string(SS_YAML_KEY_HDDUNIT) + (unit == HARDDISK_1 ? std::string("0") : std::string("1"));
	if (!yamlLoadHelper.GetSubMap(hddUnitName))
		throw std::string("Card: Expected key: ") + hddUnitName;

	g_HardDisk[unit].fullname.clear();
	g_HardDisk[unit].imagename.clear();
	g_HardDisk[unit].hd_imageloaded = false;	// Default to false (until image is successfully loaded below)
	g_HardDisk[unit].hd_status_next = DISK_STATUS_OFF;
	g_HardDisk[unit].hd_status_prev = DISK_STATUS_OFF;

	std::string filename = yamlLoadHelper.LoadString(SS_YAML_KEY_FILENAME);
	g_HardDisk[unit].hd_error = yamlLoadHelper.LoadUint(SS_YAML_KEY_ERROR);
	g_HardDisk[unit].hd_memblock = yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMBLOCK);
	g_HardDisk[unit].hd_diskblock = yamlLoadHelper.LoadUint(SS_YAML_KEY_DISKBLOCK);
	yamlLoadHelper.LoadBool(SS_YAML_KEY_IMAGELOADED);	// Consume
	Disk_Status_e diskStatusNext = (Disk_Status_e) yamlLoadHelper.LoadUint(SS_YAML_KEY_STATUS_NEXT);
	Disk_Status_e diskStatusPrev = (Disk_Status_e) yamlLoadHelper.LoadUint(SS_YAML_KEY_STATUS_PREV);
	g_HardDisk[unit].hd_buf_ptr = yamlLoadHelper.LoadUint(SS_YAML_KEY_BUF_PTR);

	if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_BUF))
		throw hddUnitName + std::string(": Missing: ") + std::string(SS_YAML_KEY_BUF);
	yamlLoadHelper.LoadMemory(g_HardDisk[unit].hd_buf, HD_BLOCK_SIZE);

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
			bResSelectImage = HD_SelectImage(unit, filename.c_str());

			dwAttributes = GetFileAttributes(filename.c_str());
		}

		bool bImageError = (dwAttributes == INVALID_FILE_ATTRIBUTES);
		if (!bImageError)
		{
			if (!HD_Insert(unit, filename.c_str()))
				bImageError = true;

			// HD_Insert() sets up:
			// . imagename
			// . fullname
			// . hd_imageloaded
			// . hd_status_next = DISK_STATUS_OFF
			// . hd_status_prev = DISK_STATUS_OFF

			g_HardDisk[unit].hd_status_next = diskStatusNext;
			g_HardDisk[unit].hd_status_prev = diskStatusPrev;
		}
	}

	return bResSelectImage;
}

bool HD_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version, const std::string strSaveStatePath)
{
	if (slot != 7)	// fixme
		throw std::string("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	if (version == 1 && (regs.pc >> 8) == (0xC0|slot))
		throw std::string("HDD card: 6502 is running old HDD firmware");

	g_nHD_UnitNum = yamlLoadHelper.LoadUint(SS_YAML_KEY_CURRENT_UNIT);	// b7=unit
	g_nHD_Command = yamlLoadHelper.LoadUint(SS_YAML_KEY_COMMAND);

	// Unplug all HDDs first in case HDD-2 is to be plugged in as HDD-1
	for (UINT i=0; i<NUM_HARDDISKS; i++)
	{
		HD_Unplug(i);
		g_HardDisk[i].clear();
	}

	bool bResSelectImage1 = HD_LoadSnapshotHDDUnit(yamlLoadHelper, HARDDISK_1);
	bool bResSelectImage2 = HD_LoadSnapshotHDDUnit(yamlLoadHelper, HARDDISK_2);

	if (!bResSelectImage1 && !bResSelectImage2)
		RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, strSaveStatePath);

	HD_SetEnabled(true);

	GetFrame().FrameRefreshStatus(DRAW_LEDS);

	return true;
}
