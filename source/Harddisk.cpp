/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
#include "HardDisk.h"
#include "DiskImage.h"	// ImageError_e
#include "DiskImageHelper.h"
#include "..\resource\resource.h"

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
	// From Disk_t
	TCHAR	imagename[16];				// Not used
	TCHAR	fullname[128];
	//
	BYTE	hd_error;
	WORD	hd_memblock;
	UINT	hd_diskblock;
	WORD	hd_buf_ptr;
	BOOL	hd_imageloaded;
	BYTE	hd_buf[HD_BLOCK_SIZE+1];	// Why +1?

	ImageInfo Info;
};

static bool	g_bHD_RomLoaded = false;
static bool g_bHD_Enabled = false;

static BYTE	g_nHD_UnitNum = HARDDISK_1;

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static BYTE	g_nHD_Command;

static HDD g_HardDisk[NUM_HARDDISKS] = {0};

static UINT g_uSlot = 7;

static CHardDiskImageHelper sg_HardDiskImageHelper;

//===========================================================================

static void HD_CleanupDrive(const int iDrive)
{
	sg_HardDiskImageHelper.Close(&g_HardDisk[iDrive].Info, false);

	g_HardDisk[iDrive].hd_imageloaded = false;
	g_HardDisk[iDrive].imagename[0] = 0;
	g_HardDisk[iDrive].fullname[0] = 0;
}

static ImageError_e ImageOpen(	LPCTSTR pszImageFilename,
								const int iDrive,
								const bool bCreateIfNecessary,
								std::string& strFilenameInZip)
{
	if (!pszImageFilename)
		return eIMAGE_ERROR_BAD_POINTER;

	HDD* pHDD = &g_HardDisk[iDrive];
	ImageInfo* pImageInfo = &pHDD->Info;
	pImageInfo->bWriteProtected = false;

	ImageError_e Err = sg_HardDiskImageHelper.Open(pszImageFilename, pImageInfo, bCreateIfNecessary, strFilenameInZip);

	if (Err != eIMAGE_ERROR_NONE)
	{
		HD_CleanupDrive(iDrive);
		return Err;
	}

	return eIMAGE_ERROR_NONE;
}

//-----------------------------------------------------------------------------

static void GetImageTitle(LPCTSTR pszImageFilename, HDD* pHardDrive)
{
	TCHAR   imagetitle[128];
	LPCTSTR startpos = pszImageFilename;
	
	// imagetitle = <FILENAME.EXT>
	if (_tcsrchr(startpos,TEXT('\\')))
		startpos = _tcsrchr(startpos,TEXT('\\'))+1;
	_tcsncpy(imagetitle,startpos,127);
	imagetitle[127] = 0;
	
	// if imagetitle contains a lowercase char, then found=1 (why?)
	BOOL found = 0;
	int  loop  = 0;
	while (imagetitle[loop] && !found)
	{
		if (IsCharLower(imagetitle[loop]))
			found = 1;
		else
			loop++;
	}
		
	if ((!found) && (loop > 2))
		CharLowerBuff(imagetitle+1,_tcslen(imagetitle+1));
	
	// hdptr->fullname = <FILENAME.EXT>
	_tcsncpy(pHardDrive->fullname,imagetitle,127);
	pHardDrive->fullname[127] = 0;
	
	if (imagetitle[0])
	{
		LPTSTR dot = imagetitle;
		if (_tcsrchr(dot,TEXT('.')))
			dot = _tcsrchr(dot,TEXT('.'));
		if (dot > imagetitle)
			*dot = 0;
	}
	
	// hdptr->imagename = <FILENAME> (ie. no extension)
	_tcsncpy(pHardDrive->imagename,imagetitle,15);
	pHardDrive->imagename[15] = 0;
}

static void NotifyInvalidImage(TCHAR* pszImageFilename)
{
	// TC: TO DO
}

static BOOL HD_Load_Image(const int iDrive, LPCSTR pszImageFilename)
{
	const bool bCreateIfNecessary = false;	// NB. Don't allow creation of HDV files
	string strFilenameInZip;				// TODO: Use this
	ImageError_e Error = ImageOpen(pszImageFilename, iDrive, bCreateIfNecessary, strFilenameInZip);

	g_HardDisk[iDrive].hd_imageloaded = (Error == eIMAGE_ERROR_NONE);

	return g_HardDisk[iDrive].hd_imageloaded;
}

//===========================================================================

// everything below is global

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

static const DWORD HDDRVR_SIZE = APPLE_SLOT_SIZE;

bool HD_CardIsEnabled(void)
{
	return g_bHD_RomLoaded && g_bHD_Enabled;
}

// Called by:
// . LoadConfiguration() - Done at each restart
// . DiskDlg_OK() - When HD is enabled/disabled on UI
void HD_SetEnabled(const bool bEnabled)
{
	if(g_bHD_Enabled == bEnabled)
		return;

	g_bHD_Enabled = bEnabled;

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
}

//-------------------------------------

LPCTSTR HD_GetFullName(const int iDrive)
{
	return g_HardDisk[iDrive].fullname;
}

LPCTSTR HD_GetFullPathName(const int iDrive)
{
	return g_HardDisk[iDrive].Info.szFilename;
}

static LPCTSTR HD_DiskGetBaseName (const int iDrive)	// Not used
{
	return g_HardDisk[iDrive].imagename;
}

//-------------------------------------

VOID HD_Load_Rom(const LPBYTE pCxRomPeripheral, const UINT uSlot)
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
}

VOID HD_Cleanup(void)
{
	for(int i=HARDDISK_1; i<HARDDISK_2; i++)
	{
		HD_CleanupDrive(i);
	}
}

// pszImageFilename is qualified with path
BOOL HD_InsertDisk(const int iDrive, LPCTSTR pszImageFilename)
{
	if (*pszImageFilename == 0x00)
		return false;

	if (g_HardDisk[iDrive].hd_imageloaded)
		HD_CleanupDrive(iDrive);

	BOOL bResult = HD_Load_Image(iDrive, pszImageFilename);

	if (bResult)
		GetImageTitle(pszImageFilename, &g_HardDisk[iDrive]);

	return bResult;
}

void HD_Select(const int iDrive)
{
	TCHAR directory[MAX_PATH] = TEXT("");
	TCHAR filename[MAX_PATH]  = TEXT("");
	TCHAR title[40];

	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, directory, MAX_PATH);
	_tcscpy(title, TEXT("Select HDV Image For HDD "));
	_tcscat(title, iDrive ? TEXT("2") : TEXT("1"));

	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = g_hFrameWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     = TEXT("Hard Disk Images (*.hdv,*.po,*.2mg,*.2img,*.gz,*.zip)\0*.hdv;*.po;*.2mg;*.2img;*.gz;*.zip\0")
						  TEXT("All Files\0*.*\0");
	ofn.lpstrFile       = filename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = directory;
	ofn.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;	// Don't allow creation & hide the read-only checkbox
	ofn.lpstrTitle      = title;

	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			_tcscat(filename,TEXT(".hdv"));
		
		if (HD_InsertDisk(iDrive, filename))
		{
			filename[ofn.nFileOffset] = 0;
			if (_tcsicmp(directory, filename))
				RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, filename);
		}
		else
		{
			NotifyInvalidImage(filename);
		}
	}
}

void HD_Unplug(const int iDrive)
{
	if (g_HardDisk[iDrive].hd_imageloaded)
		HD_CleanupDrive(iDrive);
}

bool HD_IsDriveUnplugged(const int iDrive)
{
	return g_HardDisk[iDrive].hd_imageloaded == FALSE;
}

//-----------------------------------------------------------------------------

#define DEVICE_OK				0x00
#define DEVICE_UNKNOWN_ERROR	0x03
#define DEVICE_IO_ERROR			0x08

static BYTE __stdcall HD_IO_EMUL(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	BYTE r = DEVICE_OK;
	addr &= 0xFF;

	if (!HD_CardIsEnabled())
		return r;

	HDD* pHDD = &g_HardDisk[g_nHD_UnitNum >> 7];	// bit7 = drive select

	if (bWrite == 0) // read
	{
		switch (addr)
		{
		case 0xF0:
			{
				if (pHDD->hd_imageloaded)
				{
					// based on loaded data block request, load block into memory
					// returns status
					switch (g_nHD_Command)
					{
					default:
					case 0x00: //status
						if (pHDD->Info.uImageSize == 0)
						{
							pHDD->hd_error = 1;
							r = DEVICE_IO_ERROR;
						}
						break;
					case 0x01: //read
						{
							if ((pHDD->hd_diskblock * HD_BLOCK_SIZE) < pHDD->Info.uImageSize)
							{
								bool bRes = pHDD->Info.pImageType->Read(&pHDD->Info, pHDD->hd_diskblock, pHDD->hd_buf);
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
						}
						break;
					case 0x02: //write
						{
							bool bRes = true;
							const bool bAppendBlocks = (pHDD->hd_diskblock * HD_BLOCK_SIZE) >= pHDD->Info.uImageSize;
							if (bAppendBlocks)
							{
								// TODO: Test this!
								ZeroMemory(pHDD->hd_buf, HD_BLOCK_SIZE);

								UINT uBlock = pHDD->Info.uImageSize / HD_BLOCK_SIZE;
								while (uBlock < pHDD->hd_diskblock)
								{
									bRes = pHDD->Info.pImageType->Write(&pHDD->Info, uBlock++, pHDD->hd_buf);
									_ASSERT(bRes);
									if (!bRes)
										break;
									pHDD->Info.uImageSize += HD_BLOCK_SIZE;
								}
							}

							MoveMemory(pHDD->hd_buf, mem+pHDD->hd_memblock, HD_BLOCK_SIZE);

							if (bRes)
								bRes = pHDD->Info.pImageType->Write(&pHDD->Info, pHDD->hd_diskblock, pHDD->hd_buf);

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

							if (bAppendBlocks && bRes)
								pHDD->Info.uImageSize += HD_BLOCK_SIZE;
						}
						break;
					case 0x03: //format
						break;
					}
				}
				else
				{
					pHDD->hd_error = 1;
					r = DEVICE_UNKNOWN_ERROR;
				}
			}
			break;
		case 0xF1: // hd_error
			{
				r = pHDD->hd_error;
			}
			break;
		case 0xF2:
			{
				r = g_nHD_Command;
			}
			break;
		case 0xF3:
			{
				r = g_nHD_UnitNum;
			}
			break;
		case 0xF4:
			{
				r = (BYTE)(pHDD->hd_memblock & 0x00FF);
			}
			break;
		case 0xF5:
			{
				r = (BYTE)(pHDD->hd_memblock & 0xFF00 >> 8);
			}
			break;
		case 0xF6:
			{
				r = (BYTE)(pHDD->hd_diskblock & 0x00FF);
			}
			break;
		case 0xF7:
			{
				r = (BYTE)(pHDD->hd_diskblock & 0xFF00 >> 8);
			}
			break;
		case 0xF8:
			{
				r = pHDD->hd_buf[pHDD->hd_buf_ptr];
				pHDD->hd_buf_ptr++;
			}
			break;
		default:
			return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
		}
	}
	else // write
	{
		switch (addr)
		{
		case 0xF2:
			{
				g_nHD_Command = d;
			}
			break;
		case 0xF3:
			{
				// b7    = drive#
				// b6..4 = slot#
				// b3..0 = ?
				g_nHD_UnitNum = d;
			}
			break;
		case 0xF4:
			{
				pHDD->hd_memblock = pHDD->hd_memblock & 0xFF00 | d;
			}
			break;
		case 0xF5:
			{
				pHDD->hd_memblock = pHDD->hd_memblock & 0x00FF | (d << 8);
			}
			break;
		case 0xF6:
			{
				pHDD->hd_diskblock = pHDD->hd_diskblock & 0xFF00 | d;
			}
			break;
		case 0xF7:
			{
				pHDD->hd_diskblock = pHDD->hd_diskblock & 0x00FF | (d << 8);
			}
			break;
		default:
			return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
		}
	}

	return r;
}
