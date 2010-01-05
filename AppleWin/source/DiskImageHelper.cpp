/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

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

/* Description: Disk Image Helper
 *
 * Author: Tom
 */


#include "stdafx.h"
#include "DiskImageHelper.h"
#include "DiskImage.h"


/* DO logical order  0 1 2 3 4 5 6 7 8 9 A B C D E F */
/*    physical order 0 D B 9 7 5 3 1 E C A 8 6 4 2 F */

/* PO logical order  0 E D C B A 9 8 7 6 5 4 3 2 1 F */
/*    physical order 0 2 4 6 8 A C E 1 3 5 7 9 B D F */

BYTE CImageBase::ms_DiskByte[0x40] =
{
	0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
	0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
	0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
	0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
	0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
	0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
	0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
	0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

BYTE CImageBase::ms_SectorNumber[NUM_SECTOR_ORDERS][0x10] =
{
	{0x00,0x08,0x01,0x09,0x02,0x0A,0x03,0x0B, 0x04,0x0C,0x05,0x0D,0x06,0x0E,0x07,0x0F},
	{0x00,0x07,0x0E,0x06,0x0D,0x05,0x0C,0x04, 0x0B,0x03,0x0A,0x02,0x09,0x01,0x08,0x0F},
	{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};

LPBYTE CImageBase::ms_pWorkBuffer = NULL;

//-----------------------------------------------------------------------------

bool CImageBase::ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long Offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(pTrackBuffer, &pImageInfo->pImageBuffer[Offset], uTrackSize);

	return true;
}

//-------------------------------------

bool CImageBase::WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long Offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(&pImageInfo->pImageBuffer[Offset], pTrackBuffer, uTrackSize);

	if (pImageInfo->FileType == eFileNormal)
	{
		if (pImageInfo->hFile == INVALID_HANDLE_VALUE)
			return false;

		SetFilePointer(pImageInfo->hFile, Offset, NULL, FILE_BEGIN);

		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(pImageInfo->hFile, pTrackBuffer, uTrackSize, &dwBytesWritten, NULL);
		_ASSERT(dwBytesWritten == uTrackSize);
		if (!bRes || dwBytesWritten != uTrackSize)
			return false;
	}
	else if (pImageInfo->FileType == eFileGZip)
	{
		// Write entire compressed image each time (dirty track change or dirty disk removal)
		gzFile hGZFile = gzopen(pImageInfo->szFilename, "wb");
		if (hGZFile == NULL)
			return false;

		int nLen = gzwrite(hGZFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		if (nLen != pImageInfo->uImageSize)
			return false;

		int nRes = gzclose(hGZFile);
		hGZFile = NULL;
		if (nRes != Z_OK)
			return false;
	}
	else if (pImageInfo->FileType == eFileZip)
	{
		// Write entire compressed image each time (dirty track change or dirty disk removal)
		// NB. Only support Zip archives with a single file
		zipFile hZipFile = zipOpen(pImageInfo->szFilename, APPEND_STATUS_CREATE);
		if (hZipFile == NULL)
			return false;

		int nRes = zipOpenNewFileInZip(hZipFile, pImageInfo->szFilenameInZip, &pImageInfo->zipFileInfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_SPEED);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipWriteInFileInZip(hZipFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipCloseFileInZip(hZipFile);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipClose(hZipFile, NULL);
		if (nRes != ZIP_OK)
			return false;
	}
	else
	{
		_ASSERT(0);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

bool CImageBase::ReadBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer)
{
	long Offset = pImageInfo->uOffset + nBlock * HD_BLOCK_SIZE;

	if (pImageInfo->FileType == eFileNormal)
	{
		if (pImageInfo->hFile == INVALID_HANDLE_VALUE)
			return false;

		SetFilePointer(pImageInfo->hFile, Offset, NULL, FILE_BEGIN);

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(pImageInfo->hFile, pBlockBuffer, HD_BLOCK_SIZE, &dwBytesRead, NULL);
		if (!bRes || dwBytesRead != HD_BLOCK_SIZE)
			return false;
	}
	else if ((pImageInfo->FileType == eFileGZip) || (pImageInfo->FileType == eFileZip))
	{
		memcpy(pBlockBuffer, &pImageInfo->pImageBuffer[Offset], HD_BLOCK_SIZE);
	}
	else
	{
		_ASSERT(0);
		return false;
	}

	return true;
}

//-------------------------------------

bool CImageBase::WriteBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer)
{
	long Offset = pImageInfo->uOffset + nBlock * HD_BLOCK_SIZE;

	if (pImageInfo->FileType == eFileGZip || pImageInfo->FileType == eFileZip)
	{
		if ((UINT)Offset+HD_BLOCK_SIZE > pImageInfo->uImageSize)
		{
			_ASSERT(0);
			return false;
		}

		memcpy(&pImageInfo->pImageBuffer[Offset], pBlockBuffer, HD_BLOCK_SIZE);
	}

	if (pImageInfo->FileType == eFileNormal)
	{
		if (pImageInfo->hFile == INVALID_HANDLE_VALUE)
			return false;

		SetFilePointer(pImageInfo->hFile, Offset, NULL, FILE_BEGIN);

		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(pImageInfo->hFile, pBlockBuffer, HD_BLOCK_SIZE, &dwBytesWritten, NULL);
		if (!bRes || dwBytesWritten != HD_BLOCK_SIZE)
			return false;
	}
	else if (pImageInfo->FileType == eFileGZip)
	{
		// Write entire compressed image each time a block is written
		gzFile hGZFile = gzopen(pImageInfo->szFilename, "wb");
		if (hGZFile == NULL)
			return false;

		int nLen = gzwrite(hGZFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		if (nLen != pImageInfo->uImageSize)
			return false;

		int nRes = gzclose(hGZFile);
		hGZFile = NULL;
		if (nRes != Z_OK)
			return false;
	}
	else if (pImageInfo->FileType == eFileZip)
	{
		// Write entire compressed image each time a block is written
		// NB. Only support Zip archives with a single file
		zipFile hZipFile = zipOpen(pImageInfo->szFilename, APPEND_STATUS_CREATE);
		if (hZipFile == NULL)
			return false;

		int nRes = zipOpenNewFileInZip(hZipFile, pImageInfo->szFilenameInZip, &pImageInfo->zipFileInfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_SPEED);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipWriteInFileInZip(hZipFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipCloseFileInZip(hZipFile);
		if (nRes != ZIP_OK)
			return false;

		nRes = zipClose(hZipFile, NULL);
		if (nRes != ZIP_OK)
			return false;
	}
	else
	{
		_ASSERT(0);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------

LPBYTE CImageBase::Code62(int sector)
{
	// CONVERT THE 256 8-BIT BYTES INTO 342 6-BIT BYTES, WHICH WE STORE
	// STARTING AT 4K INTO THE WORK BUFFER.
	{
		LPBYTE sectorbase = ms_pWorkBuffer+(sector << 8);
		LPBYTE resultptr  = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		BYTE   offset     = 0xAC;
		while (offset != 0x02)
		{
			BYTE value = 0;
#define ADDVALUE(a) value = (value << 2) |        \
							(((a) & 0x01) << 1) | \
							(((a) & 0x02) >> 1)
			ADDVALUE(*(sectorbase+offset));  offset -= 0x56;
			ADDVALUE(*(sectorbase+offset));  offset -= 0x56;
			ADDVALUE(*(sectorbase+offset));  offset -= 0x53;
#undef ADDVALUE
			*(resultptr++) = value << 2;
		}
		*(resultptr-2) &= 0x3F;
		*(resultptr-1) &= 0x3F;
		int loop = 0;
		while (loop < 0x100)
			*(resultptr++) = *(sectorbase+(loop++));
	}

	// EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE,
	// CREATING A 343RD BYTE WHICH IS USED AS A CHECKSUM.  STORE THE NEW
	// BLOCK OF 343 BYTES STARTING AT 5K INTO THE WORK BUFFER.
	{
		BYTE   savedval  = 0;
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		int    loop      = 342;
		while (loop--)
		{
			*(resultptr++) = savedval ^ *sourceptr;
			savedval = *(sourceptr++);
		}
		*resultptr = savedval;
	}

	// USING A LOOKUP TABLE, CONVERT THE 6-BIT BYTES INTO DISK BYTES.  A VALID
	// DISK BYTE IS A BYTE THAT HAS THE HIGH BIT SET, AT LEAST TWO ADJACENT
	// BITS SET (EXCLUDING THE HIGH BIT), AND AT MOST ONE PAIR OF CONSECUTIVE
	// ZERO BITS.  THE CONVERTED BLOCK OF 343 BYTES IS STORED STARTING AT 4K
	// INTO THE WORK BUFFER.
	{
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = ms_DiskByte[(*(sourceptr++)) >> 2];
	}

	return ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
}

//-------------------------------------

void CImageBase::Decode62(LPBYTE imageptr)
{
	// IF WE HAVEN'T ALREADY DONE SO, GENERATE A TABLE FOR CONVERTING
	// DISK BYTES BACK INTO 6-BIT BYTES
	static BOOL tablegenerated = 0;
	static BYTE sixbitbyte[0x80];
	if (!tablegenerated)
	{
		ZeroMemory(sixbitbyte,0x80);
		int loop = 0;
		while (loop < 0x40) {
			sixbitbyte[ms_DiskByte[loop]-0x80] = loop << 2;
			loop++;
		}
		tablegenerated = 1;
	}

	// USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
	{
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = sixbitbyte[*(sourceptr++) & 0x7F];
	}

	// EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
	// TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
	{
		BYTE   savedval  = 0;
		LPBYTE sourceptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 342;
		while (loop--)
		{
			*resultptr = savedval ^ *(sourceptr++);
			savedval = *(resultptr++);
		}
	}

	// CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
	{
		LPBYTE lowbitsptr = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE sectorbase = ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x56;
		BYTE   offset     = 0xAC;
		while (offset != 0x02)
		{
			if (offset >= 0xAC)
			{
				*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x80) >> 7)
										| (((*lowbitsptr) & 0x40) >> 5);
			}

			offset -= 0x56;
			*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x20) >> 5)
										| (((*lowbitsptr) & 0x10) >> 3);

			offset -= 0x56;
			*(imageptr+offset) = (*(sectorbase+offset) & 0xFC)
										| (((*lowbitsptr) & 0x08) >> 3)
										| (((*lowbitsptr) & 0x04) >> 1);

			offset -= 0x53;
			lowbitsptr++;
		}
	}
}

//-------------------------------------

void CImageBase::DenibblizeTrack(LPBYTE trackimage, SectorOrder_e SectorOrder, int nibbles)
{
	ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);

	// SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
	// WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
	// BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
	// IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
	// OFFSET BY THE SECTOR NUMBER.

	int offset    = 0;
	int partsleft = 33;
	int sector    = 0;
	while (partsleft--)
	{
		BYTE byteval[3] = {0,0,0};
		int  bytenum    = 0;
		int  loop       = nibbles;
		while ((loop--) && (bytenum < 3))
		{
			if (bytenum)
				byteval[bytenum++] = *(trackimage+offset++);
			else if (*(trackimage+offset++) == 0xD5)
				bytenum = 1;

			if (offset >= nibbles)
				offset = 0;
		}

		if ((bytenum == 3) && (byteval[1] = 0xAA))
		{
			int loop       = 0;
			int tempoffset = offset;
			while (loop < 384)
			{
				*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+loop++) = *(trackimage+tempoffset++);
				if (tempoffset >= nibbles)
					tempoffset = 0;
			}
			
			if (byteval[2] == 0x96)
			{
				sector = ((*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+4) & 0x55) << 1)
						| (*(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+5) & 0x55);
			}
			else if (byteval[2] == 0xAD)
			{
				Decode62(ms_pWorkBuffer+(ms_SectorNumber[SectorOrder][sector] << 8));
				sector = 0;
			}
		}
	}
}

//-------------------------------------

DWORD CImageBase::NibblizeTrack(LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track)
{
	ZeroMemory(ms_pWorkBuffer+TRACK_DENIBBLIZED_SIZE, TRACK_DENIBBLIZED_SIZE);
	LPBYTE imageptr = trackimagebuffer;
	BYTE   sector   = 0;

	// WRITE GAP ONE, WHICH CONTAINS 48 SELF-SYNC BYTES
	int loop;
	for (loop = 0; loop < 48; loop++)
		*(imageptr++) = 0xFF;

	while (sector < 16)
	{
		// WRITE THE ADDRESS FIELD, WHICH CONTAINS:
		//   - PROLOGUE (D5AA96)
		//   - VOLUME NUMBER ("4 AND 4" ENCODED)
		//   - TRACK NUMBER ("4 AND 4" ENCODED)
		//   - SECTOR NUMBER ("4 AND 4" ENCODED)
		//   - CHECKSUM ("4 AND 4" ENCODED)
		//   - EPILOGUE (DEAAEB)
		*(imageptr++) = 0xD5;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0x96;
#define CODE44A(a) ((((a) >> 1) & 0x55) | 0xAA)
#define CODE44B(a) (((a) & 0x55) | 0xAA)
		*(imageptr++) = CODE44A(m_uVolumeNumber);
		*(imageptr++) = CODE44B(m_uVolumeNumber);
		*(imageptr++) = CODE44A((BYTE)track);
		*(imageptr++) = CODE44B((BYTE)track);
		*(imageptr++) = CODE44A(sector);
		*(imageptr++) = CODE44B(sector);
		*(imageptr++) = CODE44A(m_uVolumeNumber ^ ((BYTE)track) ^ sector);
		*(imageptr++) = CODE44B(m_uVolumeNumber ^ ((BYTE)track) ^ sector);
#undef CODE44A
#undef CODE44B
		*(imageptr++) = 0xDE;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xEB;

		// WRITE GAP TWO, WHICH CONTAINS SIX SELF-SYNC BYTES
		for (loop = 0; loop < 6; loop++)
			*(imageptr++) = 0xFF;

		// WRITE THE DATA FIELD, WHICH CONTAINS:
		//   - PROLOGUE (D5AAAD)
		//   - 343 6-BIT BYTES OF NIBBLIZED DATA, INCLUDING A 6-BIT CHECKSUM
		//   - EPILOGUE (DEAAEB)
		*(imageptr++) = 0xD5;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xAD;
		CopyMemory(imageptr, Code62(ms_SectorNumber[SectorOrder][sector]), 343);
		imageptr += 343;
		*(imageptr++) = 0xDE;
		*(imageptr++) = 0xAA;
		*(imageptr++) = 0xEB;

		// WRITE GAP THREE, WHICH CONTAINS 27 SELF-SYNC BYTES
		for (loop = 0; loop < 27; loop++)
			*(imageptr++) = 0xFF;

		sector++;
	}

	return imageptr-trackimagebuffer;
}

//-------------------------------------

void CImageBase::SkewTrack(const int nTrack, const int nNumNibbles, const LPBYTE pTrackImageBuffer)
{
	int nSkewBytes = (nTrack*768) % nNumNibbles;
	CopyMemory(ms_pWorkBuffer, pTrackImageBuffer, nNumNibbles);
	CopyMemory(pTrackImageBuffer, ms_pWorkBuffer+nSkewBytes, nNumNibbles-nSkewBytes);
	CopyMemory(pTrackImageBuffer+nNumNibbles-nSkewBytes, ms_pWorkBuffer, nSkewBytes);
}

//-------------------------------------

bool CImageBase::IsValidImageSize(const DWORD uImageSize)
{
	m_uNumTracksInImage = 0;

	if ((TRACKS_MAX>TRACKS_STANDARD) && (uImageSize > TRACKS_MAX*TRACK_DENIBBLIZED_SIZE))
		return false;	// >160KB

	//

	bool bValidSize = false;

	if (uImageSize >= (TRACKS_STANDARD+1)*TRACK_DENIBBLIZED_SIZE)
	{
		// Is uImageSize == 140KB + n*4K?	(where n>=1)
		bValidSize = (((uImageSize - TRACKS_STANDARD*TRACK_DENIBBLIZED_SIZE) % TRACK_DENIBBLIZED_SIZE) == 0);
	}
	else
	{
		// TODO: Applewin.chm mentions images of size 143,616 bytes ("Disk Image Formats")
		bValidSize = (  ((uImageSize >= 143105) && (uImageSize <= 143364)) ||
						 (uImageSize == 143403) ||
						 (uImageSize == 143488) );
	}

	if (bValidSize)
		m_uNumTracksInImage = uImageSize / TRACK_DENIBBLIZED_SIZE;

	return bValidSize;
}

//===========================================================================

// DOS ORDER (DO) FORMAT IMPLEMENTATION
class CDoImage : public CImageBase
{
public:
	CDoImage(void) {}
	virtual ~CDoImage(void) {}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (!IsValidImageSize(dwImageSize))
			return eMismatch;

		// CHECK FOR A DOS ORDER IMAGE OF A DOS DISKETTE
		{
			int  loop      = 0;
			bool bMismatch = false;
			while ((loop++ < 15) && !bMismatch)
			{
				if (*(pImage+0x11002+(loop << 8)) != loop-1)
					bMismatch = true;
			}
			if (!bMismatch)
				return eMatch;
		}

		// CHECK FOR A DOS ORDER IMAGE OF A PRODOS DISKETTE
		{
			int  loop      = 1;
			bool bMismatch = false;
			while ((loop++ < 5) && !bMismatch)
			{
				if ((*(LPWORD)(pImage+(loop << 9)+0x100) != ((loop == 5) ? 0 : 6-loop)) ||
					(*(LPWORD)(pImage+(loop << 9)+0x102) != ((loop == 2) ? 0 : 8-loop)))
					bMismatch = 1;
			}
			if (!bMismatch)
				return eMatch;
		}

		return ePossibleMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eDOSOrder, nTrack);
		if (!enhancedisk)
			SkewTrack(nTrack, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		DenibblizeTrack(pTrackImage, eDOSOrder, nNibbles);
		WriteTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetImageSizeForCreate(void) { return TRACK_DENIBBLIZED_SIZE * TRACKS_STANDARD; }

	virtual eImageType GetType(void) { return eImageDO; }
	virtual char* GetCreateExtensions(void) { return ".do;.dsk"; }
	virtual char* GetRejectExtensions(void) { return ".nib;.iie;.po;.prg"; }
};

//-------------------------------------

// PRODOS ORDER (PO) FORMAT IMPLEMENTATION
class CPoImage : public CImageBase
{
public:
	CPoImage(void) {}
	virtual ~CPoImage(void) {}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (!IsValidImageSize(dwImageSize))
			return eMismatch;

		// CHECK FOR A PRODOS ORDER IMAGE OF A DOS DISKETTE
		{
			int  loop      = 4;
			bool bMismatch = false;
			while ((loop++ < 13) && !bMismatch)
				if (*(pImage+0x11002+(loop << 8)) != 14-loop)
					bMismatch = true;
			if (!bMismatch)
				return eMatch;
		}

		// CHECK FOR A PRODOS ORDER IMAGE OF A PRODOS DISKETTE
		{
			int  loop      = 1;
			bool bMismatch = false;
			while ((loop++ < 5) && !bMismatch)
			{
				if ((*(LPWORD)(pImage+(loop << 9)  ) != ((loop == 2) ? 0 : loop-1)) ||
					(*(LPWORD)(pImage+(loop << 9)+2) != ((loop == 5) ? 0 : loop+1)))
					bMismatch = true;
			}
			if (!bMismatch)
				return eMatch;
		}

		return ePossibleMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eProDOSOrder, nTrack);
		if (!enhancedisk)
			SkewTrack(nTrack, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		DenibblizeTrack(pTrackImage, eProDOSOrder, nNibbles);
		WriteTrack(pImageInfo, nTrack, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual eImageType GetType(void) { return eImagePO; }
	virtual char* GetCreateExtensions(void) { return ".po"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.nib;.prg"; }
};

//-------------------------------------

// NIBBLIZED 6656-NIBBLE (NIB) FORMAT IMPLEMENTATION
class CNib1Image : public CImageBase
{
public:
	CNib1Image(void) {}
	virtual ~CNib1Image(void) {}

	static const UINT NIB1_TRACK_SIZE = NIBBLES_PER_TRACK;

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (dwImageSize != NIB1_TRACK_SIZE*TRACKS_STANDARD)
			return eMismatch;

		m_uNumTracksInImage = TRACKS_STANDARD;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, pTrackImageBuffer, NIB1_TRACK_SIZE);
		*pNibbles = NIB1_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		_ASSERT(nNibbles == NIB1_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		WriteTrack(pImageInfo, nTrack, pTrackImage, nNibbles);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetImageSizeForCreate(void) { return NIB1_TRACK_SIZE * TRACKS_STANDARD; }

	virtual eImageType GetType(void) { return eImageNIB1; }
	virtual char* GetCreateExtensions(void) { return ".nib"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg"; }
};

//-------------------------------------

// NIBBLIZED 6384-NIBBLE (NB2) FORMAT IMPLEMENTATION
class CNib2Image : public CImageBase
{
public:
	CNib2Image(void) {}
	virtual ~CNib2Image(void) {}

	static const UINT NIB2_TRACK_SIZE = 6384;

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (dwImageSize != NIB2_TRACK_SIZE*TRACKS_STANDARD)
			return eMismatch;

		m_uNumTracksInImage = TRACKS_STANDARD;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		ReadTrack(pImageInfo, nTrack, pTrackImageBuffer, NIB2_TRACK_SIZE);
		*pNibbles = NIB2_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		_ASSERT(nNibbles == NIB2_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		WriteTrack(pImageInfo, nTrack, pTrackImage, nNibbles);
	}

	virtual eImageType GetType(void) { return eImageNIB2; }
	virtual char* GetCreateExtensions(void) { return ".nb2"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg;.2mg;.2img"; }
};

//-------------------------------------

// HDV image
class CHDVImage : public CImageBase
{
public:
	CHDVImage(void) {}
	virtual ~CHDVImage(void) {}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		m_uNumTracksInImage = dwImageSize / TRACK_DENIBBLIZED_SIZE;	// Set to non-zero

		// An HDV image can be any size (so if Ext == ".hdv" then accept any size)
		if (*pszExt && !_tcscmp(pszExt, ".hdv"))
			return eMatch;

		if (dwImageSize < UNIDISK35_800K_SIZE)
			return eMismatch;

		return eMatch;
	}

	virtual bool Read(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer)
	{
		return ReadBlock(pImageInfo, nBlock, pBlockBuffer);
	}

	virtual bool Write(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer)
	{
		if (pImageInfo->bWriteProtected)
			return false;

		return WriteBlock(pImageInfo, nBlock, pBlockBuffer);
	}

	virtual eImageType GetType(void) { return eImageHDV; }
	virtual char* GetCreateExtensions(void) { return ".hdv"; }
	virtual char* GetRejectExtensions(void) { return ".do;.iie;.prg"; }
};

//-------------------------------------

// SIMSYSTEM IIE (IIE) FORMAT IMPLEMENTATION
class CIIeImage : public CImageBase
{
public:
	CIIeImage(void) : m_pHeader(NULL) {}
	virtual ~CIIeImage(void) { delete [] m_pHeader; }

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (strncmp((const char *)pImage, "SIMSYSTEM_IIE", 13) || (*(pImage+13) > 3))
			return eMismatch;

		m_uNumTracksInImage = TRACKS_STANDARD;	// Assume default # tracks
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles)
	{
		// IF WE HAVEN'T ALREADY DONE SO, READ THE IMAGE FILE HEADER
		if (!m_pHeader)
		{
			m_pHeader = (LPBYTE) VirtualAlloc(NULL, 88, MEM_COMMIT, PAGE_READWRITE);
			if (!m_pHeader)
			{
				*pNibbles = 0;
				return;
			}
			ZeroMemory(m_pHeader, 88);
			DWORD dwBytesRead;
			SetFilePointer(pImageInfo->hFile, 0, NULL,FILE_BEGIN);
			ReadFile(pImageInfo->hFile, m_pHeader, 88, &dwBytesRead, NULL);
		}

		// IF THIS IMAGE CONTAINS USER DATA, READ THE TRACK AND NIBBLIZE IT
		if (*(m_pHeader+13) <= 2)
		{
			ConvertSectorOrder(m_pHeader+14);
			SetFilePointer(pImageInfo->hFile, nTrack*TRACK_DENIBBLIZED_SIZE+30, NULL, FILE_BEGIN);
			ZeroMemory(ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
			DWORD bytesread;
			ReadFile(pImageInfo->hFile, ms_pWorkBuffer, TRACK_DENIBBLIZED_SIZE, &bytesread, NULL);
			*pNibbles = NibblizeTrack(pTrackImageBuffer, eSIMSYSTEMOrder, nTrack);
		}
		// OTHERWISE, IF THIS IMAGE CONTAINS NIBBLE INFORMATION, READ IT DIRECTLY INTO THE TRACK BUFFER
		else 
		{
			*pNibbles = *(LPWORD)(m_pHeader+nTrack*2+14);
			LONG Offset = 88;
			while (nTrack--)
				Offset += *(LPWORD)(m_pHeader+nTrack*2+14);
			SetFilePointer(pImageInfo->hFile, Offset, NULL,FILE_BEGIN);
			ZeroMemory(pTrackImageBuffer, *pNibbles);
			DWORD dwBytesRead;
			ReadFile(pImageInfo->hFile, pTrackImageBuffer, *pNibbles, &dwBytesRead, NULL);
		}
	}

	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles)
	{
		// note: unimplemented
	}

	virtual eImageType GetType(void) { return eImageIIE; }
	virtual char* GetCreateExtensions(void) { return ".iie"; }
	virtual char* GetRejectExtensions(void) { return ".do.;.nib;.po;.prg;.2mg;.2img"; }

private:
	void ConvertSectorOrder(LPBYTE sourceorder)
	{
		int loop = 16;
		while (loop--)
		{
			BYTE found = 0xFF;
			int  loop2 = 16;
			while (loop2-- && (found == 0xFF))
			{
				if (*(sourceorder+loop2) == loop)
					found = loop2;
			}

			if (found == 0xFF)
				found = 0;

			ms_SectorNumber[2][loop] = found;
		}
	}

private:
	LPBYTE m_pHeader;
};

//-------------------------------------

// RAW PROGRAM IMAGE (APL) FORMAT IMPLEMENTATION
class CAplImage : public CImageBase
{
public:
	CAplImage(void) {}
	virtual ~CAplImage(void) {}

	virtual bool Boot(ImageInfo* ptr)
	{
		SetFilePointer(ptr->hFile, 0, NULL, FILE_BEGIN);
		WORD address = 0;
		WORD length  = 0;
		DWORD bytesread;
		ReadFile(ptr->hFile, &address, sizeof(WORD), &bytesread, NULL);
		ReadFile(ptr->hFile, &length , sizeof(WORD), &bytesread, NULL);
		if ((((WORD)(address+length)) <= address) ||
			(address >= 0xC000) ||
			(address+length-1 >= 0xC000))
		{
			return false;
		}

		ReadFile(ptr->hFile, mem+address, length, &bytesread, NULL);
		int loop = 192;
		while (loop--)
			*(memdirty+loop) = 0xFF;

		regs.pc = address;
		return true;
	}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		DWORD dwLength = *(LPWORD)(pImage+2);
		bool bRes = (((dwLength+4) == dwImageSize) ||
					((dwLength+4+((256-((dwLength+4) & 255)) & 255)) == dwImageSize));

		return !bRes ? eMismatch : ePossibleMatch;
	}

	virtual bool AllowBoot(void) { return true; }
	virtual bool AllowRW(void) { return false; }

	virtual eImageType GetType(void) { return eImageAPL; }
	virtual char* GetCreateExtensions(void) { return ".apl"; }
	virtual char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po;.2mg;.2img"; }
};

//-------------------------------------

class CPrgImage : public CImageBase
{
public:
	CPrgImage(void) {}
	virtual ~CPrgImage(void) {}

	virtual bool Boot(ImageInfo* pImageInfo)
	{
		SetFilePointer(pImageInfo->hFile, 5, NULL, FILE_BEGIN);

		WORD address = 0;
		WORD length  = 0;
		DWORD bytesread;

		ReadFile(pImageInfo->hFile, &address, sizeof(WORD), &bytesread, NULL);
		ReadFile(pImageInfo->hFile, &length , sizeof(WORD), &bytesread, NULL);

		length <<= 1;
		if ((((WORD)(address+length)) <= address) ||
			(address >= 0xC000) ||
			(address+length-1 >= 0xC000))
		{
			return false;
		}

		SetFilePointer(pImageInfo->hFile,128,NULL,FILE_BEGIN);
		ReadFile(pImageInfo->hFile, mem+address, length, &bytesread, NULL);

		int loop = 192;
		while (loop--)
			*(memdirty+loop) = 0xFF;

		regs.pc = address;
		return true;
	}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		return (*(LPDWORD)pImage == 0x214C470A) ? eMatch : eMismatch;	// "!LG\x0A"
	}

	virtual bool AllowBoot(void) { return true; }
	virtual bool AllowRW(void) { return false; }

	virtual eImageType GetType(void) { return eImagePRG; }
	virtual char* GetCreateExtensions(void) { return ".prg"; }
	virtual char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po;.2mg;.2img"; }
};

//-----------------------------------------------------------------------------

eDetectResult CMacBinaryHelper::DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset)
{
	// DETERMINE WHETHER THE FILE HAS A 128-BYTE MACBINARY HEADER
	if ((dwImageSize > uMacBinHdrSize) &&
		(!*pImage) &&
		(*(pImage+1) < 120) &&
		(!*(pImage+*(pImage+1)+2)) &&
		(*(pImage+0x7A) == 0x81) &&
		(*(pImage+0x7B) == 0x81))
	{
		pImage += uMacBinHdrSize;
		dwImageSize -= uMacBinHdrSize;
		dwOffset = uMacBinHdrSize;
		return eMatch;
	}

	return eMismatch;
}

eDetectResult C2IMGHelper::DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset)
{
	Header2IMG* pHdr = (Header2IMG*) pImage;

	if (dwImageSize < sizeof(Header2IMG) || pHdr->FormatID != FormatID_2IMG || pHdr->HeaderSize != sizeof(Header2IMG))
		return eMismatch;

	if (pHdr->Version != 1)
		return eMismatch;

	if (dwImageSize < sizeof(Header2IMG)+pHdr->DiskDataLength)
		return eMismatch;

	//

	pImage += sizeof(Header2IMG);
	dwImageSize = pHdr->DiskDataLength;
	dwOffset += sizeof(Header2IMG);

	switch (pHdr->ImageFormat)
	{
	case e2IMGFormatDOS33:
		{
			if (pHdr->DiskDataLength < TRACKS_STANDARD*TRACK_DENIBBLIZED_SIZE)
				return eMismatch;
		}
		break;
	case e2IMGFormatProDOS:
		{
			// FYI: GS image: PassengersontheWind.2mg has DiskDataLength==0
			dwImageSize = pHdr->DiskDataLength ? pHdr->DiskDataLength : pHdr->NumBlocks * HD_BLOCK_SIZE;

			if (m_bIsFloppy)
			{
				if (dwImageSize < TRACKS_STANDARD*TRACK_DENIBBLIZED_SIZE)
					return eMismatch;

				if (dwImageSize <= TRACKS_MAX*TRACK_DENIBBLIZED_SIZE)
					break;

				if (dwImageSize == UNIDISK35_800K_SIZE)	// TODO: For //c+,  eg. Prince of Persia (Original 3.5 floppy for IIc+).2MG
					return eMismatch;

				return eMismatch;
			}
		}
		break;
	case e2IMGFormatNIBData:
		{
			if (pHdr->DiskDataLength != TRACKS_STANDARD*NIBBLES_PER_TRACK)
				return eMismatch;
		}
		break;
	default:
		return eMismatch;
	}

	memcpy(&m_Hdr, pHdr, sizeof(m_Hdr));
	return eMatch;
}

BYTE C2IMGHelper::GetVolumeNumber(void)
{
	if (m_Hdr.ImageFormat != e2IMGFormatDOS33 || !m_Hdr.Flags.bDOS33VolumeNumberValid)
		return 254;

	return m_Hdr.Flags.VolumeNumber;
}

bool C2IMGHelper::IsLocked(void)
{
	return m_Hdr.Flags.bDiskImageLocked;
}

//-----------------------------------------------------------------------------

// NB. Of the 6 cases (floppy/harddisk x gzip/zip/normal) only harddisk-normal isn't read entirely to memory
// - harddisk-normal-create also doesn't create a max size image-buffer

// DETERMINE THE FILE'S EXTENSION AND CONVERT IT TO LOWERCASE
void GetCharLowerExt(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize)
{
	LPCTSTR pImageFileExt = pszImageFilename;

	if (_tcsrchr(pImageFileExt, TEXT('\\')))
		pImageFileExt = _tcsrchr(pImageFileExt, TEXT('\\'))+1;

	if (_tcsrchr(pImageFileExt, TEXT('.')))
		pImageFileExt = _tcsrchr(pImageFileExt, TEXT('.'));

	_tcsncpy(pszExt, pImageFileExt, uExtSize);

	CharLowerBuff(pszExt, _tcslen(pszExt));
}

void GetCharLowerExt2(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize)
{
	TCHAR szFilename[MAX_PATH];
	_tcsncpy(szFilename, pszImageFilename, MAX_PATH);

	TCHAR* pLastDot = _tcsrchr(szFilename, TEXT('.'));
	if (pLastDot)
		*pLastDot = 0;

	GetCharLowerExt(pszExt, szFilename, uExtSize);
}

//-----------------

ImageError_e CImageHelperBase::CheckGZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo)
{
	gzFile hGZFile = gzopen(pszImageFilename, "rb");
	if (hGZFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_GZ;

	const UINT MAX_UNCOMPRESSED_SIZE = GetMaxImageSize() + 1;	// +1 to detect images that are too big
	pImageInfo->pImageBuffer = new BYTE[MAX_UNCOMPRESSED_SIZE];
	if (!pImageInfo->pImageBuffer)
		return eIMAGE_ERROR_BAD_POINTER;

	int nLen = gzread(hGZFile, pImageInfo->pImageBuffer, MAX_UNCOMPRESSED_SIZE);
	if (nLen < 0 || nLen == MAX_UNCOMPRESSED_SIZE)
		return eIMAGE_ERROR_BAD_SIZE;

	int nRes = gzclose(hGZFile);
	hGZFile = NULL;
	if (nRes != Z_OK)
		return eIMAGE_ERROR_GZ;

	//

	// Strip .gz then try to determine the file's extension and convert it to lowercase
	TCHAR szExt[_MAX_EXT] = "";
	GetCharLowerExt2(szExt, pszImageFilename, _MAX_EXT);

	DWORD dwSize = nLen;
	DWORD dwOffset = 0;
	CImageBase* pImageType = Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset, &pImageInfo->bWriteProtected);

	if (!pImageType)
		return eIMAGE_ERROR_UNSUPPORTED;

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	pImageInfo->FileType = eFileGZip;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

ImageError_e CImageHelperBase::CheckZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, std::string& strFilenameInZip)
{
	zlib_filefunc_def ffunc;
	fill_win32_filefunc(&ffunc);		// TODO: Ditch this and use unzOpen() instead?

	unzFile hZipFile = unzOpen2(pszImageFilename, &ffunc);
	if (hZipFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP;

	unz_global_info global_info;
	int nRes = unzGetGlobalInfo(hZipFile, &global_info);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	nRes = unzGoToFirstFile(hZipFile);	// Only support 1st file in zip archive for now
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	unz_file_info file_info;
	char szFilename[MAX_PATH];
	memset(szFilename, 0, sizeof(szFilename));
	nRes = unzGetCurrentFileInfo(hZipFile, &file_info, szFilename, MAX_PATH, NULL, 0, NULL, 0);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	const UINT uFileSize = file_info.uncompressed_size;
	if (uFileSize > GetMaxImageSize())
		return eIMAGE_ERROR_BAD_SIZE;

	pImageInfo->pImageBuffer = new BYTE[uFileSize];
	if (!pImageInfo->pImageBuffer)
		return eIMAGE_ERROR_BAD_POINTER;

	nRes = unzOpenCurrentFile(hZipFile);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	int nLen = unzReadCurrentFile(hZipFile, pImageInfo->pImageBuffer, uFileSize);
	if (nLen < 0)
	{
		unzCloseCurrentFile(hZipFile);	// Must CloseCurrentFile before Close
		return eIMAGE_ERROR_UNSUPPORTED;
	}

	nRes = unzCloseCurrentFile(hZipFile);
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	nRes = unzClose(hZipFile);
	hZipFile = NULL;
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	strncpy(pImageInfo->szFilenameInZip, szFilename, MAX_PATH);
	memcpy(&pImageInfo->zipFileInfo.tmz_date, &file_info.tmu_date, sizeof(file_info.tmu_date));
	pImageInfo->zipFileInfo.dosDate     = file_info.dosDate;
	pImageInfo->zipFileInfo.internal_fa = file_info.internal_fa;
	pImageInfo->zipFileInfo.external_fa = file_info.external_fa;
	pImageInfo->uNumEntriesInZip = global_info.number_entry;
	strFilenameInZip = szFilename;

	//

	// Determine the file's extension and convert it to lowercase
	TCHAR szExt[_MAX_EXT] = "";
	GetCharLowerExt(szExt, szFilename, _MAX_EXT);

	DWORD dwSize = nLen;
	DWORD dwOffset = 0;
	CImageBase* pImageType = Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset, &pImageInfo->bWriteProtected);

	if (!pImageType)
	{
		if (global_info.number_entry > 1)
			return eIMAGE_ERROR_UNSUPPORTED_MULTI_ZIP;

		return eIMAGE_ERROR_UNSUPPORTED;
	}

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	if (global_info.number_entry > 1)
		pImageInfo->bWriteProtected = 1;	// Zip archives with multiple files are read-only (for now)

	pImageInfo->FileType = eFileZip;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

ImageError_e CImageHelperBase::CheckNormalFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, const bool bCreateIfNecessary)
{
	// TRY TO OPEN THE IMAGE FILE

	HANDLE& hFile = pImageInfo->hFile;

	if (!pImageInfo->bWriteProtected)
	{
		hFile = CreateFile(pszImageFilename,
                      GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ,
                      (LPSECURITY_ATTRIBUTES)NULL,
                      OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL,
                      NULL);
	}

	// File may have read-only attribute set, so try to open as read-only.
	if (hFile == INVALID_HANDLE_VALUE)
	{
		hFile = CreateFile(
			pszImageFilename,
			GENERIC_READ,
			FILE_SHARE_READ,
			(LPSECURITY_ATTRIBUTES)NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL );
		
		if (hFile != INVALID_HANDLE_VALUE)
			pImageInfo->bWriteProtected = 1;
	}

	if ((hFile == INVALID_HANDLE_VALUE) && bCreateIfNecessary)
		hFile = CreateFile(
			pszImageFilename,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,
			(LPSECURITY_ATTRIBUTES)NULL,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL,
			NULL );

	// IF WE AREN'T ABLE TO OPEN THE FILE, RETURN
	if (hFile == INVALID_HANDLE_VALUE)
		return eIMAGE_ERROR_UNABLE_TO_OPEN;

	// Determine the file's extension and convert it to lowercase
	TCHAR szExt[_MAX_EXT] = "";
	GetCharLowerExt(szExt, pszImageFilename, _MAX_EXT);

	DWORD dwSize = GetFileSize(hFile, NULL);
	DWORD dwOffset = 0;
	CImageBase* pImageType = NULL;

	if (dwSize > 0)
	{
		if (dwSize > GetMaxImageSize())
			return eIMAGE_ERROR_BAD_SIZE;

		bool bTempDetectBuffer;
		const UINT uDetectSize = GetMinDetectSize(dwSize, &bTempDetectBuffer);

		pImageInfo->pImageBuffer = new BYTE [dwSize];
		if (!pImageInfo->pImageBuffer)
			return eIMAGE_ERROR_BAD_POINTER;

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(hFile, pImageInfo->pImageBuffer, dwSize, &dwBytesRead, NULL);
		if (!bRes || dwSize != dwBytesRead)
		{
			delete [] pImageInfo->pImageBuffer;
			pImageInfo->pImageBuffer = NULL;
			return eIMAGE_ERROR_BAD_SIZE;
		}

		pImageType = Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset, &pImageInfo->bWriteProtected);
		if (bTempDetectBuffer)
		{
			delete [] pImageInfo->pImageBuffer;
			pImageInfo->pImageBuffer = NULL;
		}
	}
	else	// Create (or pre-existing zero-length file)
	{
		pImageType = GetImageForCreation(szExt, &dwSize);
		if (pImageType && dwSize)
		{
			pImageInfo->pImageBuffer = new BYTE [dwSize];
			if (!pImageInfo->pImageBuffer)
				return eIMAGE_ERROR_BAD_POINTER;

			ZeroMemory(pImageInfo->pImageBuffer, dwSize);
		}
	}

	//

	if (!pImageType)
	{
		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;

		if (dwSize == 0)
			DeleteFile(pszImageFilename);

		return eIMAGE_ERROR_UNSUPPORTED;
	}

	pImageInfo->FileType = eFileNormal;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

ImageError_e CImageHelperBase::Open(	LPCTSTR pszImageFilename,
										ImageInfo* pImageInfo,
										const bool bCreateIfNecessary,
										std::string& strFilenameInZip)
{
	pImageInfo->hFile = INVALID_HANDLE_VALUE;

	ImageError_e Err;
    const size_t uStrLen = strlen(pszImageFilename);

    if (uStrLen > GZ_SUFFIX_LEN && strcmp(pszImageFilename+uStrLen-GZ_SUFFIX_LEN, GZ_SUFFIX) == 0)
	{
		Err = CheckGZipFile(pszImageFilename, pImageInfo);
	}
    else if (uStrLen > ZIP_SUFFIX_LEN && strcmp(pszImageFilename+uStrLen-ZIP_SUFFIX_LEN, ZIP_SUFFIX) == 0)
	{
		Err = CheckZipFile(pszImageFilename, pImageInfo, strFilenameInZip);
	}
	else
	{
		Err = CheckNormalFile(pszImageFilename, pImageInfo, bCreateIfNecessary);
	}

	if (pImageInfo->pImageType == NULL && Err == eIMAGE_ERROR_NONE)
		Err = eIMAGE_ERROR_UNSUPPORTED;

	if (Err != eIMAGE_ERROR_NONE)
		return Err;

	_tcsncpy(pImageInfo->szFilename, pszImageFilename, MAX_PATH);

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

void CImageHelperBase::Close(ImageInfo* pImageInfo, const bool bDeleteFile)
{
	if (pImageInfo->hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pImageInfo->hFile);
		pImageInfo->hFile = INVALID_HANDLE_VALUE;
	}

	if (bDeleteFile)
	{
		DeleteFile(pImageInfo->szFilename);
		pImageInfo->szFilename[0] = 0;
	}

	delete [] pImageInfo->pImageBuffer;
	pImageInfo->pImageBuffer = NULL;
}

//-----------------------------------------------------------------------------

CDiskImageHelper::CDiskImageHelper(void) :
	CImageHelperBase(true)
{
	m_vecImageTypes.push_back( new CDoImage );
	m_vecImageTypes.push_back( new CPoImage );
	m_vecImageTypes.push_back( new CNib1Image );
	m_vecImageTypes.push_back( new CNib2Image );
	m_vecImageTypes.push_back( new CHDVImage );		// Used to trap inserting a small .hdv (or a non-140K .2mg) into a floppy drive! Small means <GetMaxFloppyImageSize()
	m_vecImageTypes.push_back( new CIIeImage );
	m_vecImageTypes.push_back( new CAplImage );
	m_vecImageTypes.push_back( new CPrgImage );
}

CImageBase* CDiskImageHelper::Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_)
{
	dwOffset = 0;
	m_MacBinaryHelper.DetectHdr(pImage, dwSize, dwOffset);
	m_Result2IMG = m_2IMGHelper.DetectHdr(pImage, dwSize, dwOffset);

	// CALL THE DETECTION FUNCTIONS IN ORDER, LOOKING FOR A MATCH
	eImageType ImageType = eImageUNKNOWN;
	eImageType PossibleType = eImageUNKNOWN;

	for (UINT uLoop=0; uLoop < GetNumImages() && ImageType == eImageUNKNOWN; uLoop++)
	{
		if (*pszExt && _tcsstr(GetImage(uLoop)->GetRejectExtensions(), pszExt))
			continue;

		eDetectResult Result = GetImage(uLoop)->Detect(pImage, dwSize, pszExt);
		if (Result == eMatch)
			ImageType = GetImage(uLoop)->GetType();
		else if ((Result == ePossibleMatch) && (PossibleType == eImageUNKNOWN))
			PossibleType = GetImage(uLoop)->GetType();
	}

	if (ImageType == eImageUNKNOWN)
		ImageType = PossibleType;

	CImageBase* pImageType = GetImage(ImageType);

	if (pImageType)
	{
		if (pImageType->AllowRW())
		{
			const UINT uNumTracksInImage = GetNumTracksInImage(pImageType);
			_ASSERT(uNumTracksInImage);	// Should've been set by Image's Detect()
			if (uNumTracksInImage == 0)
				SetNumTracksInImage(pImageType, (dwSize > 0) ? TRACKS_STANDARD : 0);	// Assume default # tracks
		}

		if (m_Result2IMG == eMatch)
		{
			pImageType->m_uVolumeNumber = m_2IMGHelper.GetVolumeNumber();

			if (m_2IMGHelper.IsLocked() && !*pWriteProtected_)
				*pWriteProtected_ = 1;
		}
	}

	return pImageType;
}

CImageBase* CDiskImageHelper::GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize)
{
	// WE CREATE ONLY DOS ORDER (DO) OR 6656-NIBBLE (NIB) FORMAT FILES
	for (UINT uLoop = 0; uLoop < GetNumImages(); uLoop++)
	{
		if (!GetImage(uLoop)->AllowCreate())
			continue;

		if (*pszExt && _tcsstr(GetImage(uLoop)->GetCreateExtensions(), pszExt))
		{
			CImageBase* pImageType = GetImage(uLoop);
			SetNumTracksInImage(pImageType, TRACKS_STANDARD);	// Assume default # tracks

			*pCreateImageSize = pImageType->GetImageSizeForCreate();
			if (*pCreateImageSize == (UINT)-1)
				return NULL;

			return pImageType;
		}
	}

	return NULL;
}

UINT CDiskImageHelper::GetMaxImageSize(void)
{
	// TODO: This doesn't account for .2mg files with comments after the disk-image
	return UNIDISK35_800K_SIZE + m_MacBinaryHelper.GetMaxHdrSize() + m_2IMGHelper.GetMaxHdrSize();
}

UINT CDiskImageHelper::GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer)
{
	*pTempDetectBuffer = false;
	return uImageSize;
}

//-----------------------------------------------------------------------------

CHardDiskImageHelper::CHardDiskImageHelper(void) :
	CImageHelperBase(false)
{
	m_vecImageTypes.push_back( new CHDVImage );
}

CImageBase* CHardDiskImageHelper::Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_)
{
	dwOffset = 0;
	m_Result2IMG = m_2IMGHelper.DetectHdr(pImage, dwSize, dwOffset);

	eImageType ImageType = eImageUNKNOWN;

	for (UINT uLoop=0; uLoop < GetNumImages() && ImageType == eImageUNKNOWN; uLoop++)
	{
		if (*pszExt && _tcsstr(GetImage(uLoop)->GetRejectExtensions(), pszExt))
			continue;

		eDetectResult Result = GetImage(uLoop)->Detect(pImage, dwSize, pszExt);
		if (Result == eMatch)
			ImageType = GetImage(uLoop)->GetType();

		_ASSERT(Result != ePossibleMatch);
	}

	CImageBase* pImageType = GetImage(ImageType);

	if (pImageType)
	{
		if (m_Result2IMG == eMatch)
		{
			if (m_2IMGHelper.IsLocked() && !*pWriteProtected_)
				*pWriteProtected_ = 1;
		}
	}

	return pImageType;
}

CImageBase* CHardDiskImageHelper::GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize)
{
	// NB. Not supported for HardDisks
	// - Would need to create a default 16-block file like CiderPress

	for (UINT uLoop = 0; uLoop < GetNumImages(); uLoop++)
	{
		if (!GetImage(uLoop)->AllowCreate())
			continue;

		if (*pszExt && _tcsstr(GetImage(uLoop)->GetCreateExtensions(), pszExt))
		{
			CImageBase* pImageType = GetImage(uLoop);

			*pCreateImageSize = pImageType->GetImageSizeForCreate();
			if (*pCreateImageSize == (UINT)-1)
				return NULL;

			return pImageType;
		}
	}

	return NULL;
}

UINT CHardDiskImageHelper::GetMaxImageSize(void)
{
	// TODO: This doesn't account for .2mg files with comments after the disk-image
	return HARDDISK_32M_SIZE + m_2IMGHelper.GetMaxHdrSize();
}

UINT CHardDiskImageHelper::GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer)
{
	*pTempDetectBuffer = true;
	return m_2IMGHelper.GetMaxHdrSize();
}
