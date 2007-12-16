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
#pragma  hdrstop
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
      ways they weren't designed (like telling Ultima 5 that you have a Phazor
      sound card in slot 7 is a generally bad idea) will cause problems.
*/

typedef struct
{
	TCHAR	hd_imagename[16];
	TCHAR	hd_fullname[128];
	BYTE	hd_error;
	WORD	hd_memblock;
	WORD	hd_diskblock;
	WORD	hd_buf_ptr;
	BOOL	hd_imageloaded;
	HANDLE  hd_file;
	BYTE	hd_buf[513];
} HDD, *PHDD;

static bool	g_bHD_RomLoaded = false;
static bool g_bHD_Enabled = false;

static BYTE	g_nHD_UnitNum = DRIVE_1;

// The HDD interface has a single Command register for both drives:
// . ProDOS will write to Command before switching drives
static BYTE	g_nHD_Command;

static HDD g_HardDrive[2] = {0};

static UINT g_uSlot = 7;

//===========================================================================

static void GetImageTitle (LPCTSTR imagefilename, PHDD pHardDrive)
{
	TCHAR   imagetitle[128];
	LPCTSTR startpos = imagefilename;
	
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
	
	// fptr->fullname = <FILENAME.EXT>
	_tcsncpy(pHardDrive->hd_fullname,imagetitle,127);
	pHardDrive->hd_fullname[127] = 0;
	
	if (imagetitle[0])
	{
		LPTSTR dot = imagetitle;
		if (_tcsrchr(dot,TEXT('.')))
			dot = _tcsrchr(dot,TEXT('.'));
		if (dot > imagetitle)
			*dot = 0;
	}
	
	// fptr->imagename = <FILENAME> (ie. no extension)
	_tcsncpy(pHardDrive->hd_imagename,imagetitle,15);
	pHardDrive->hd_imagename[15] = 0;
}

static void NotifyInvalidImage (TCHAR* filename)
{
	// TC: TO DO
}

static void HD_CleanupDrive(int nDrive)
{
	CloseHandle(g_HardDrive[nDrive].hd_file);
	g_HardDrive[nDrive].hd_imageloaded = false;
	g_HardDrive[nDrive].hd_imagename[0] = 0;
	g_HardDrive[nDrive].hd_fullname[0] = 0;
}

static BOOL HD_Load_Image(int nDrive, LPCSTR filename)
{
	g_HardDrive[nDrive].hd_file = CreateFile(filename,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ,
		(LPSECURITY_ATTRIBUTES)NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
		NULL);

	if (g_HardDrive[nDrive].hd_file == INVALID_HANDLE_VALUE)
		g_HardDrive[nDrive].hd_imageloaded = false;
	else
		g_HardDrive[nDrive].hd_imageloaded = true;

	return g_HardDrive[nDrive].hd_imageloaded;
}

static LPCTSTR HD_DiskGetName (int nDrive)
{
	return g_HardDrive[nDrive].hd_imagename;
}

//===========================================================================

// everything below is global

static BYTE __stdcall HD_IO_EMUL (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

static const DWORD HDDRVR_SIZE = 0x100;

bool HD_CardIsEnabled()
{
	return g_bHD_RomLoaded && g_bHD_Enabled;
}

// Called by:
// . LoadConfiguration() - Done at each restart
// . DiskDlg_OK() - When HD is enabled/disabled on UI
void HD_SetEnabled(bool bEnabled)
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

LPCTSTR HD_GetFullName (int nDrive)
{
	return g_HardDrive[nDrive].hd_fullname;
}

VOID HD_Load_Rom(LPBYTE pCxRomPeripheral, UINT uSlot)
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

VOID HD_Cleanup()
{
	for(int i=DRIVE_1; i<DRIVE_2; i++)
	{
		HD_CleanupDrive(i);
	}
}

// pszFilename is not qualified with path
BOOL HD_InsertDisk2(int nDrive, LPCTSTR pszFilename)
{
	if (*pszFilename == 0x00)
		return false;

	char szFullFilename[MAX_PATH];

	RegLoadString(TEXT("Preferences"),TEXT("HDV Starting Directory"), 1, szFullFilename, MAX_PATH);
	strcat(szFullFilename, pszFilename);

	return HD_InsertDisk(nDrive, szFullFilename);
}

// imagefilename is qualified with path
BOOL HD_InsertDisk(int nDrive, LPCTSTR imagefilename)
{
  if (*imagefilename == 0x00)
    return false;

  if (g_HardDrive[nDrive].hd_imageloaded)
    HD_CleanupDrive(nDrive);

  BOOL result = HD_Load_Image(nDrive, imagefilename);

  if (result)
    GetImageTitle(imagefilename, &g_HardDrive[nDrive]);

  return result;
}

void HD_Select(int nDrive)
{
	TCHAR directory[MAX_PATH] = TEXT("");
	TCHAR filename[MAX_PATH]  = TEXT("");
	TCHAR title[40];

	RegLoadString(TEXT("Preferences"),TEXT("HDV Starting Directory"),1,directory,MAX_PATH);
	_tcscpy(title,TEXT("Select HDV Image For HDD "));
	_tcscat(title,nDrive ? TEXT("2") : TEXT("1"));
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = g_hFrameWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     = TEXT("Hard Disk Images (*.hdv,*.po)\0*.hdv;*.po\0")
						  TEXT("All Files\0*.*\0");
	ofn.lpstrFile       = filename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = directory;
	ofn.Flags           = OFN_CREATEPROMPT | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = title;
	ofn.lpTemplateName  = TEXT("INSERT_DIALOG");
	
	if (GetOpenFileName(&ofn))
	{
		if ((!ofn.nFileExtension) || !filename[ofn.nFileExtension])
			_tcscat(filename,TEXT(".hdv"));
		
		if (HD_InsertDisk(nDrive, filename))
		{
			filename[ofn.nFileOffset] = 0;
			if (_tcsicmp(directory,filename))
				RegSaveString(TEXT("Preferences"),TEXT("HDV Starting Directory"),1,filename);
		}
		else
		{
			NotifyInvalidImage(filename);
		}
	}
}

//-----------------------------------------------------------------------------

#define DEVICE_OK				0x00
#define DEVICE_UNKNOWN_ERROR	0x03
#define DEVICE_IO_ERROR			0x08

static BYTE __stdcall HD_IO_EMUL (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	BYTE r = DEVICE_OK;
	addr &= 0xFF;

	if (!HD_CardIsEnabled())
		return r;

	PHDD pHDD = &g_HardDrive[g_nHD_UnitNum >> 7];	// bit7 = drive select

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
						if (GetFileSize(pHDD->hd_file,NULL) == 0)
						{
							pHDD->hd_error = 1;
							r = DEVICE_IO_ERROR;
						}
						break;
					case 0x01: //read
						{
							DWORD br = GetFileSize(pHDD->hd_file,NULL);
							if ((DWORD)(pHDD->hd_diskblock * 512) <= br)	// seek to block
							{
								SetFilePointer(pHDD->hd_file,pHDD->hd_diskblock * 512,NULL,FILE_BEGIN);	// seek to block
								if (ReadFile(pHDD->hd_file,pHDD->hd_buf,512,&br,NULL))	// read block into buffer
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
							DWORD bw = GetFileSize(pHDD->hd_file,NULL);
							if ((DWORD)(pHDD->hd_diskblock * 512) <= bw)
							{
								MoveMemory(pHDD->hd_buf,mem+pHDD->hd_memblock,512);
								SetFilePointer(pHDD->hd_file,pHDD->hd_diskblock * 512,NULL,FILE_BEGIN);	// seek to block
								if (WriteFile(pHDD->hd_file,pHDD->hd_buf,512,&bw,NULL))	// write buffer to file
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
							else
							{
								DWORD fsize = SetFilePointer(pHDD->hd_file,0,NULL,FILE_END);
								DWORD addblocks = pHDD->hd_diskblock - (fsize / 512);
								FillMemory(pHDD->hd_buf,512,0);
								while (addblocks--)
								{
									DWORD bw;
									WriteFile(pHDD->hd_file,pHDD->hd_buf,512,&bw,NULL);
								}
								if (SetFilePointer(pHDD->hd_file,pHDD->hd_diskblock * 512,NULL,FILE_BEGIN) != 0xFFFFFFFF) {	// seek to block
									MoveMemory(pHDD->hd_buf,mem+pHDD->hd_memblock,512);
									if (WriteFile(pHDD->hd_file,pHDD->hd_buf,512,&bw,NULL)) // write buffer to file
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
							}
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
