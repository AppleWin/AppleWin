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


#include "StdAfx.h"
#include "Core.h"
#include "DiskImageHelper.h"

#include "Common.h"

#include "zlib.h"
#include "unzip.h"

#include "CPU.h"
#include "DiskImage.h"
#include "Log.h"
#include "Memory.h"

ImageInfo::ImageInfo()
{
	// this is not a POD as it contains c++ strings
	// simply zeroing is not going to work
	pImageType = NULL;
	pImageHelper = NULL;
	FileType = eFileNormal;
	hFile = INVALID_HANDLE_VALUE;
	uOffset = 0;
	bWriteProtected = false;
	uImageSize = 0;
	memset(&zipFileInfo, 0, sizeof(zipFileInfo));
	uNumEntriesInZip = 0;
	uNumValidImagesInZip = 0;
	uNumTracks = 0;
	pImageBuffer = NULL;
	pWOZTrackMap = NULL;
	optimalBitTiming = 0;
	bootSectorFormat = CWOZHelper::bootUnknown;
	maxNibblesPerTrack = 0;
}

CImageBase::CImageBase()
	: m_uNumTracksInImage(0)
	, m_uVolumeNumber(DEFAULT_VOLUME_NUMBER)
{
	m_pWorkBuffer = new BYTE[TRACK_DENIBBLIZED_SIZE * 2];
}

CImageBase::~CImageBase()
{
	delete [] m_pWorkBuffer;
	m_pWorkBuffer = NULL;
}


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

//-----------------------------------------------------------------------------

bool CImageBase::WriteImageHeader(ImageInfo* pImageInfo, LPBYTE pHdr, const UINT hdrSize)
{
	return WriteImageData(pImageInfo, pHdr, hdrSize, 0);
}

//-----------------------------------------------------------------------------

bool CImageBase::ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(pTrackBuffer, &pImageInfo->pImageBuffer[offset], uTrackSize);

	return true;
}

//-------------------------------------

bool CImageBase::WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize)
{
	const long offset = pImageInfo->uOffset + nTrack * uTrackSize;
	memcpy(&pImageInfo->pImageBuffer[offset], pTrackBuffer, uTrackSize);

	return WriteImageData(pImageInfo, pTrackBuffer, uTrackSize, offset);
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
	long offset = pImageInfo->uOffset + nBlock * HD_BLOCK_SIZE;
	const bool bGrowImageBuffer = (UINT)offset+HD_BLOCK_SIZE > pImageInfo->uImageSize;

	if (pImageInfo->FileType == eFileGZip || pImageInfo->FileType == eFileZip)
	{
		if (bGrowImageBuffer)
		{
			// Horribly inefficient! (Unzip to a normal file if you want better performance!)
			const UINT uNewImageSize = offset+HD_BLOCK_SIZE;
			BYTE* pNewImageBuffer = new BYTE [uNewImageSize];

			memcpy(pNewImageBuffer, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
			memset(&pNewImageBuffer[pImageInfo->uImageSize], 0, uNewImageSize-pImageInfo->uImageSize);	// Should always be HD_BLOCK_SIZE (so this is redundant)

			delete [] pImageInfo->pImageBuffer;
			pImageInfo->pImageBuffer = pNewImageBuffer;
			pImageInfo->uImageSize = uNewImageSize;
		}

		memcpy(&pImageInfo->pImageBuffer[offset], pBlockBuffer, HD_BLOCK_SIZE);
	}

	if (!WriteImageData(pImageInfo, pBlockBuffer, HD_BLOCK_SIZE, offset))
	{
		_ASSERT(0);
		return false;
	}

	if (pImageInfo->FileType == eFileNormal)
	{
		if (bGrowImageBuffer)
			pImageInfo->uImageSize += HD_BLOCK_SIZE;
	}

	return true;
}

//-----------------------------------------------------------------------------

bool CImageBase::WriteImageData(ImageInfo* pImageInfo, LPBYTE pSrcBuffer, const UINT uSrcSize, const long offset)
{
	if (pImageInfo->FileType == eFileNormal)
	{
		if (pImageInfo->hFile == INVALID_HANDLE_VALUE)
			return false;

		if (SetFilePointer(pImageInfo->hFile, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
		{
			DWORD err = GetLastError();
			return false;
		}

		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(pImageInfo->hFile, pSrcBuffer, uSrcSize, &dwBytesWritten, NULL);
		_ASSERT(dwBytesWritten == uSrcSize);
		if (!bRes || dwBytesWritten != uSrcSize)
			return false;
	}
	else if (pImageInfo->FileType == eFileGZip)
	{
		// Write entire compressed image each time (dirty track change or dirty disk removal or a HDD block is written)
		gzFile hGZFile = gzopen(pImageInfo->szFilename.c_str(), "wb");
		if (hGZFile == NULL)
			return false;

		int nLen = gzwrite(hGZFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
		int nRes = gzclose(hGZFile);	// close before returning (due to error) to avoid resource leak
		hGZFile = NULL;

		if (nLen != pImageInfo->uImageSize)
			return false;

		if (nRes != Z_OK)
			return false;
	}
	else if (pImageInfo->FileType == eFileZip)
	{
		// Write entire compressed image each time (dirty track change or dirty disk removal or a HDD block is written)
		// NB. Only support Zip archives with a single file
		// - there is no delete in a zipfile, so would need to copy files from old to new zip file!
		_ASSERT(pImageInfo->uNumEntriesInZip == 1);	// Should never occur, since image will be write-protected in CheckZipFile()
		if (pImageInfo->uNumEntriesInZip > 1)
			return false;

		zipFile hZipFile = zipOpen(pImageInfo->szFilename.c_str(), APPEND_STATUS_CREATE);
		if (hZipFile == NULL)
			return false;

		int nOpenedFileInZip = ZIP_BADZIPFILE;

		try
		{
			nOpenedFileInZip = zipOpenNewFileInZip(hZipFile, pImageInfo->szFilenameInZip.c_str(), &pImageInfo->zipFileInfo, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_BEST_SPEED);
			if (nOpenedFileInZip != ZIP_OK)
				throw false;

			int nRes = zipWriteInFileInZip(hZipFile, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
			if (nRes != ZIP_OK)
				throw false;

			nOpenedFileInZip = ZIP_BADZIPFILE;
			nRes = zipCloseFileInZip(hZipFile);
			if (nRes != ZIP_OK)
				throw false;
		}
		catch (bool)
		{
			if (nOpenedFileInZip == ZIP_OK)
				zipCloseFileInZip(hZipFile);

			zipClose(hZipFile, NULL);

			return false;
		}

		int nRes = zipClose(hZipFile, NULL);
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
		LPBYTE sectorbase = m_pWorkBuffer+(sector << 8);
		LPBYTE resultptr  = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
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
		LPBYTE sourceptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
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
		LPBYTE sourceptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = ms_DiskByte[(*(sourceptr++)) >> 2];
	}

	return m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
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
		memset(sixbitbyte, 0, 0x80);
		int loop = 0;
		while (loop < 0x40) {
			sixbitbyte[ms_DiskByte[loop]-0x80] = loop << 2;
			loop++;
		}
		tablegenerated = 1;
	}

	// USING OUR TABLE, CONVERT THE DISK BYTES BACK INTO 6-BIT BYTES
	{
		LPBYTE sourceptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE resultptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		int    loop      = 343;
		while (loop--)
			*(resultptr++) = sixbitbyte[*(sourceptr++) & 0x7F];
	}

	// EXCLUSIVE-OR THE ENTIRE DATA BLOCK WITH ITSELF OFFSET BY ONE BYTE
	// TO UNDO THE EFFECTS OF THE CHECKSUMMING PROCESS
	{
		BYTE   savedval  = 0;
		LPBYTE sourceptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x400;
		LPBYTE resultptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		int    loop      = 342;
		while (loop--)
		{
			*resultptr = savedval ^ *(sourceptr++);
			savedval = *(resultptr++);
		}
	}

	// CONVERT THE 342 6-BIT BYTES INTO 256 8-BIT BYTES
	{
		LPBYTE lowbitsptr = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE;
		LPBYTE sectorbase = m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+0x56;
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
	memset(m_pWorkBuffer, 0, TRACK_DENIBBLIZED_SIZE);

	// SEARCH THROUGH THE TRACK IMAGE FOR EACH SECTOR.  FOR EVERY SECTOR
	// WE FIND, COPY THE NIBBLIZED DATA FOR THAT SECTOR INTO THE WORK
	// BUFFER AT OFFSET 4K.  THEN CALL DECODE62() TO DENIBBLIZE THE DATA
	// IN THE BUFFER AND WRITE IT INTO THE FIRST PART OF THE WORK BUFFER
	// OFFSET BY THE SECTOR NUMBER.

#ifdef _DEBUG
	UINT16 bmWrittenSectorAddrFields = 0x0000;
	BYTE uWriteDataFieldPrologueCount = 0;
#endif

	int offset    = 0;
	int partsleft = NUM_SECTORS*2+1;	// TC: 32+1 prologues - need 1 extra if trackimage starts between Addr Field & Data Field
	int sector    = -1;
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

		if ((bytenum == 3) && (byteval[1] == 0xAA))
		{
			int loop       = 0;
			int tempoffset = offset;
			while (loop < 384)	// TODO-TC: Why 384? Only need 343 for Decode62()
			{
				*(m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+loop++) = *(trackimage+tempoffset++);
				if (tempoffset >= nibbles)
					tempoffset = 0;
			}

			if (byteval[2] == 0x96)
			{
				sector = ((*(m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+4) & 0x55) << 1)
						| (*(m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE+5) & 0x55);

#ifdef _DEBUG
				_ASSERT( sector < NUM_SECTORS );
				if (partsleft != 0)
				{
					_ASSERT( (bmWrittenSectorAddrFields & (1<<sector)) == 0 );
					bmWrittenSectorAddrFields |= (1<<sector);
				}
#endif
			}
			else if (byteval[2] == 0xAD)
			{
				if (sector >= 0 && sector < NUM_SECTORS)
				{
#ifdef _DEBUG
					uWriteDataFieldPrologueCount++;
					_ASSERT(uWriteDataFieldPrologueCount <= NUM_SECTORS);
#endif
					Decode62(m_pWorkBuffer+(ms_SectorNumber[SectorOrder][sector] << 8));
				}
				sector = 0;
			}
		}
	}
}

//-------------------------------------

DWORD CImageBase::NibblizeTrack(LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track)
{
	memset(m_pWorkBuffer+TRACK_DENIBBLIZED_SIZE, 0, TRACK_DENIBBLIZED_SIZE);
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
		memcpy(imageptr, Code62(ms_SectorNumber[SectorOrder][sector]), 343);
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
	memcpy(m_pWorkBuffer, pTrackImageBuffer, nNumNibbles);
	memcpy(pTrackImageBuffer, m_pWorkBuffer+nSkewBytes, nNumNibbles-nSkewBytes);
	memcpy(pTrackImageBuffer+nNumNibbles-nSkewBytes, m_pWorkBuffer, nSkewBytes);
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
					bMismatch = true;
			}
			if (!bMismatch)
				return eMatch;
		}

		return ePossibleMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		const UINT track = PhaseToTrack(phase);
		ReadTrack(pImageInfo, track, m_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eDOSOrder, track);
		if (!enhanceDisk)
			SkewTrack(track, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		const UINT track = PhaseToTrack(phase);
		DenibblizeTrack(pTrackImageBuffer, eDOSOrder, nNibbles);
		WriteTrack(pImageInfo, track, m_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetImageSizeForCreate(void) { m_uNumTracksInImage = TRACKS_STANDARD; return TRACK_DENIBBLIZED_SIZE * TRACKS_STANDARD; }

	virtual eImageType GetType(void) { return eImageDO; }
	virtual const char* GetCreateExtensions(void) { return ".do;.dsk"; }
	virtual const char* GetRejectExtensions(void) { return ".nib;.iie;.po;.prg"; }
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
			{
				if (*(pImage+0x11002+(loop << 8)) != 14-loop)
					bMismatch = true;
			}
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

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		const UINT track = PhaseToTrack(phase);
		ReadTrack(pImageInfo, track, m_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
		*pNibbles = NibblizeTrack(pTrackImageBuffer, eProDOSOrder, track);
		if (!enhanceDisk)
			SkewTrack(track, *pNibbles, pTrackImageBuffer);
	}

	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		const UINT track = PhaseToTrack(phase);
		DenibblizeTrack(pTrackImageBuffer, eProDOSOrder, nNibbles);
		WriteTrack(pImageInfo, track, m_pWorkBuffer, TRACK_DENIBBLIZED_SIZE);
	}

	virtual eImageType GetType(void) { return eImagePO; }
	virtual const char* GetCreateExtensions(void) { return ".po"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.iie;.nib;.prg;.woz"; }
};

//-------------------------------------

// NIBBLIZED 6656-NIBBLE (NIB) FORMAT IMPLEMENTATION
class CNib1Image : public CImageBase
{
public:
	CNib1Image(void) {}
	virtual ~CNib1Image(void) {}

	static const UINT NIB1_TRACK_SIZE = NIBBLES_PER_TRACK_NIB;

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		if (dwImageSize < NIB1_TRACK_SIZE*TRACKS_STANDARD || dwImageSize % NIB1_TRACK_SIZE != 0 || dwImageSize > NIB1_TRACK_SIZE*TRACKS_MAX)
			return eMismatch;

		m_uNumTracksInImage = dwImageSize / NIB1_TRACK_SIZE;

		for (UINT track=0; track<m_uNumTracksInImage; track++)	// Quick NIB sanity check (GH#139)
		{
			BYTE* pTrack = &pImage[track*NIB1_TRACK_SIZE];
			for (UINT byte=0; byte<NIB1_TRACK_SIZE; byte++)
			{
				if (pTrack[byte] != 0xD5)	// find 1st D5 in track
					continue;
				UINT prologueHdr = 0;
				for (int i=0; i<3; i++)
				{
					prologueHdr <<= 8;
					prologueHdr |= pTrack[byte++];
					if (byte == NIB1_TRACK_SIZE) byte = 0;
				}
				if (prologueHdr != 0xD5AA96 && prologueHdr != 0xD5AAB5)	// ProDOS/DOS 3.3 or DOS 3.2
				{
					std::string warning = "Warning: T$%02X: NIB image's first D5 header isn't D5AA96 or D5AAB5 (found: %06X)\n";
					LogOutput(warning.c_str(), track, prologueHdr);
					LogFileOutput(warning.c_str(), track, prologueHdr);
				}
				break;
			}
		}

		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		const UINT track = PhaseToTrack(phase);
		ReadTrack(pImageInfo, track, pTrackImageBuffer, NIB1_TRACK_SIZE);
		*pNibbles = NIB1_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		_ASSERT(nNibbles == NIB1_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		const UINT track = PhaseToTrack(phase);
		WriteTrack(pImageInfo, track, pTrackImageBuffer, nNibbles);
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetImageSizeForCreate(void) { m_uNumTracksInImage = TRACKS_STANDARD; return NIB1_TRACK_SIZE * TRACKS_STANDARD; }

	virtual eImageType GetType(void) { return eImageNIB1; }
	virtual const char* GetCreateExtensions(void) { return ".nib"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg;.woz"; }
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

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		const UINT track = PhaseToTrack(phase);
		ReadTrack(pImageInfo, track, pTrackImageBuffer, NIB2_TRACK_SIZE);
		*pNibbles = NIB2_TRACK_SIZE;
	}

	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		_ASSERT(nNibbles == NIB2_TRACK_SIZE);	// Must be true - as nNibbles gets init'd by ImageReadTrace()
		const UINT track = PhaseToTrack(phase);
		WriteTrack(pImageInfo, track, pTrackImageBuffer, nNibbles);
	}

	virtual eImageType GetType(void) { return eImageNIB2; }
	virtual const char* GetCreateExtensions(void) { return ".nb2"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.iie;.po;.prg;.woz;.2mg;.2img"; }
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
	virtual const char* GetCreateExtensions(void) { return ".hdv"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.iie;.prg"; }
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

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		UINT track = PhaseToTrack(phase);

		// IF WE HAVEN'T ALREADY DONE SO, READ THE IMAGE FILE HEADER
		if (!m_pHeader)
		{
			m_pHeader = new BYTE[88];
			memset(m_pHeader, 0, 88);
			DWORD dwBytesRead;
			SetFilePointer(pImageInfo->hFile, 0, NULL,FILE_BEGIN);
			ReadFile(pImageInfo->hFile, m_pHeader, 88, &dwBytesRead, NULL);
		}

		// IF THIS IMAGE CONTAINS USER DATA, READ THE TRACK AND NIBBLIZE IT
		if (*(m_pHeader+13) <= 2)
		{
			ConvertSectorOrder(m_pHeader+14);
			SetFilePointer(pImageInfo->hFile, track*TRACK_DENIBBLIZED_SIZE+30, NULL, FILE_BEGIN);
			memset(m_pWorkBuffer, 0, TRACK_DENIBBLIZED_SIZE);
			DWORD bytesread;
			ReadFile(pImageInfo->hFile, m_pWorkBuffer, TRACK_DENIBBLIZED_SIZE, &bytesread, NULL);
			*pNibbles = NibblizeTrack(pTrackImageBuffer, eSIMSYSTEMOrder, track);
		}
		// OTHERWISE, IF THIS IMAGE CONTAINS NIBBLE INFORMATION, READ IT DIRECTLY INTO THE TRACK BUFFER
		else 
		{
			*pNibbles = *(LPWORD)(m_pHeader+track*2+14);
			LONG Offset = 88;
			while (track--)
				Offset += *(LPWORD)(m_pHeader+track*2+14);
			SetFilePointer(pImageInfo->hFile, Offset, NULL,FILE_BEGIN);
			memset(pTrackImageBuffer, 0, *pNibbles);
			DWORD dwBytesRead;
			ReadFile(pImageInfo->hFile, pTrackImageBuffer, *pNibbles, &dwBytesRead, NULL);
		}
	}

	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		// note: unimplemented
	}

	virtual eImageType GetType(void) { return eImageIIE; }
	virtual const char* GetCreateExtensions(void) { return ".iie"; }
	virtual const char* GetRejectExtensions(void) { return ".do.;.nib;.po;.prg;.woz;.2mg;.2img"; }

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
	virtual const char* GetCreateExtensions(void) { return ".apl"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po;.woz;.2mg;.2img"; }
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
	virtual const char* GetCreateExtensions(void) { return ".prg"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.dsk;.iie;.nib;.po;.woz;.2mg;.2img"; }
};

//-------------------------------------

class CWOZImageHelper
{
public:
	CWOZImageHelper(void)
	{
		m_pWOZEmptyTrack = new BYTE[CWOZHelper::EMPTY_TRACK_SIZE];

		srand(1);	// Use a fixed seed for determinism
		for (UINT i = 0; i < CWOZHelper::EMPTY_TRACK_SIZE; i++)
		{
			BYTE n = 0;
			for (UINT j = 0; j < 8; j++)
			{
				if (rand() < RAND_THRESHOLD(3, 10))	// ~30% of buffer are 1 bits
					n |= 1 << j;
			}
			m_pWOZEmptyTrack[i] = n;
		}
	}
	virtual ~CWOZImageHelper(void) { delete [] m_pWOZEmptyTrack; }

	void ReadEmptyTrack(LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount)
	{
		memcpy(pTrackImageBuffer, m_pWOZEmptyTrack, CWOZHelper::EMPTY_TRACK_SIZE);
		*pNibbles = CWOZHelper::EMPTY_TRACK_SIZE;
		*pBitCount = CWOZHelper::EMPTY_TRACK_SIZE * 8;
		return;
	}

	bool UpdateWOZHeaderCRC(ImageInfo* pImageInfo, CImageBase* pImageBase, UINT extendedSize)
	{
		BYTE* pImage = pImageInfo->pImageBuffer;
		CWOZHelper::WOZHeader* pWozHdr = (CWOZHelper::WOZHeader*) pImage;
		pWozHdr->crc32 = crc32(0, pImage+sizeof(CWOZHelper::WOZHeader), pImageInfo->uImageSize-sizeof(CWOZHelper::WOZHeader));
		return pImageBase->WriteImageHeader(pImageInfo, pImage, sizeof(CWOZHelper::WOZHeader)+extendedSize);
	}

private:
	BYTE* m_pWOZEmptyTrack;
};

//-------------------------------------

class CWOZ1Image : public CImageBase, private CWOZImageHelper
{
public:
	CWOZ1Image(void) {}
	virtual ~CWOZ1Image(void) {}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		CWOZHelper::WOZHeader* pWozHdr = (CWOZHelper::WOZHeader*) pImage;

		if (pWozHdr->id1 != CWOZHelper::ID1_WOZ1 || pWozHdr->id2 != CWOZHelper::ID2)
			return eMismatch;

		m_uNumTracksInImage = CWOZHelper::MAX_TRACKS_5_25;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		BYTE* pTrackMap = ((CWOZHelper::Tmap*)pImageInfo->pWOZTrackMap)->tmap;

		const BYTE indexFromTMAP = pTrackMap[(UINT)(phase * 2)];
		if (indexFromTMAP == CWOZHelper::TMAP_TRACK_EMPTY)
			return ReadEmptyTrack(pTrackImageBuffer, pNibbles, pBitCount);

		ReadTrack(pImageInfo, indexFromTMAP, pTrackImageBuffer, CWOZHelper::WOZ1_TRACK_SIZE);
		CWOZHelper::TRKv1* pTRK = (CWOZHelper::TRKv1*) &pTrackImageBuffer[CWOZHelper::WOZ1_TRK_OFFSET];
		*pBitCount = pTRK->bitCount;
		*pNibbles = pTRK->bytesUsed;
	}

	// TODO: support writing a bitCount (ie. fractional nibbles)
	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		if (nNibbles > CWOZHelper::WOZ1_TRK_OFFSET)
		{
			_ASSERT(0);
			LogFileOutput("WOZ1 Write Track: failed - track too big (%08X, phase=%f) for file: %s\n", nNibbles, phase, pImageInfo->szFilename.c_str());
			return;
		}

		UINT hdrExtendedSize = 0;
		const UINT trkExtendedSize = CWOZHelper::WOZ1_TRACK_SIZE;
		BYTE* pTrackMap = ((CWOZHelper::Tmap*)pImageInfo->pWOZTrackMap)->tmap;

		BYTE indexFromTMAP = pTrackMap[(UINT)(phase * 2)];
		if (indexFromTMAP == CWOZHelper::TMAP_TRACK_EMPTY)
		{
			const BYTE track = (BYTE)(phase*2);
			{
				int highestIdx = -1;
				for (UINT i=0; i<CWOZHelper::MAX_QUARTER_TRACKS_5_25; i++)
				{
					if (pTrackMap[i] != CWOZHelper::TMAP_TRACK_EMPTY && pTrackMap[i] > highestIdx)
						highestIdx = pTrackMap[i];
				}
				indexFromTMAP = (highestIdx == -1) ? 0 : highestIdx+1;
			}
			pTrackMap[track] = indexFromTMAP;
			if (track-1 >= 0) pTrackMap[track-1] = indexFromTMAP;	// WOZ spec: track is also visible from neighboring quarter tracks
			if (track+1 < CWOZHelper::MAX_QUARTER_TRACKS_5_25)	pTrackMap[track+1] = indexFromTMAP;

			const UINT newImageSize = pImageInfo->uImageSize + trkExtendedSize;
			BYTE* pNewImageBuffer = new BYTE[newImageSize];
			memcpy(pNewImageBuffer, pImageInfo->pImageBuffer, pImageInfo->uImageSize);

			// NB. delete old pImageBuffer: pWOZTrackMap updated in WOZUpdateInfo() by parent function

			delete [] pImageInfo->pImageBuffer;
			pTrackMap = NULL;	// invalidate
			pImageInfo->pImageBuffer = pNewImageBuffer;
			pImageInfo->uImageSize = newImageSize;

			// NB. pTrackImageBuffer[] is at least WOZ1_TRACK_SIZE in size
			memset(&pTrackImageBuffer[nNibbles], 0, CWOZHelper::WOZ1_TRACK_SIZE-nNibbles);
			CWOZHelper::TRKv1* pTRK = (CWOZHelper::TRKv1*) &pTrackImageBuffer[CWOZHelper::WOZ1_TRK_OFFSET];
			pTRK->bytesUsed = nNibbles;
			pTRK->bitCount = nNibbles * 8;

			CWOZHelper::WOZChunkHdr* pTrksHdr = (CWOZHelper::WOZChunkHdr*) &pImageInfo->pImageBuffer[pImageInfo->uOffset - sizeof(CWOZHelper::WOZChunkHdr)];
			pTrksHdr->size += trkExtendedSize;

			hdrExtendedSize = pImageInfo->uOffset - sizeof(CWOZHelper::WOZHeader);
		}

		// NB. pTrackImageBuffer[] is at least WOZ1_TRACK_SIZE in size
		{
			CWOZHelper::TRKv1* pTRK = (CWOZHelper::TRKv1*) &pTrackImageBuffer[CWOZHelper::WOZ1_TRK_OFFSET];
			UINT bitCount = pTRK->bitCount;
			UINT trackSize = pTRK->bytesUsed;
			_ASSERT(trackSize == nNibbles);
			if (trackSize != nNibbles)
			{
				_ASSERT(0);
				LogFileOutput("WOZ1 Write Track: (warning) attempting to write %08X when trackSize is %08X (phase=%f)\n", nNibbles, trackSize, phase);
				// NB. just a warning, not a failure (therefore nNibbles < WOZ1_TRK_OFFSET, due to check at start of function)
			}
		}

		if (!WriteTrack(pImageInfo, indexFromTMAP, pTrackImageBuffer, trkExtendedSize))
		{
			_ASSERT(0);
			LogFileOutput("WOZ1 Write Track: failed to write track (phase=%f) for file: %s\n", phase, pImageInfo->szFilename.c_str());
			return;
		}

		// TODO: zip/gzip: combine the track & hdr writes so that the file is only compressed & written once
		if (!UpdateWOZHeaderCRC(pImageInfo, this, hdrExtendedSize))
		{
			_ASSERT(0);
			LogFileOutput("WOZ1 Write Track: failed to write header CRC for file: %s\n", pImageInfo->szFilename.c_str());
		}
	}

	virtual eImageType GetType(void) { return eImageWOZ1; }
	virtual const char* GetCreateExtensions(void) { return ".woz"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.dsk;.nib;.iie;.po;.prg"; }
};

//-------------------------------------

class CWOZ2Image : public CImageBase, private CWOZImageHelper
{
public:
	CWOZ2Image(void) {}
	virtual ~CWOZ2Image(void) {}

	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt)
	{
		CWOZHelper::WOZHeader* pWozHdr = (CWOZHelper::WOZHeader*) pImage;

		if (pWozHdr->id1 != CWOZHelper::ID1_WOZ2 || pWozHdr->id2 != CWOZHelper::ID2)
			return eMismatch;

		m_uNumTracksInImage = CWOZHelper::MAX_TRACKS_5_25;
		return eMatch;
	}

	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk)
	{
		BYTE* pTrackMap = ((CWOZHelper::Tmap*)pImageInfo->pWOZTrackMap)->tmap;

		const BYTE indexFromTMAP = pTrackMap[(BYTE)(phase * 2)];
		if (indexFromTMAP == CWOZHelper::TMAP_TRACK_EMPTY)
			return ReadEmptyTrack(pTrackImageBuffer, pNibbles, pBitCount);

		CWOZHelper::TRKv2* pTRKS = (CWOZHelper::TRKv2*) &pImageInfo->pImageBuffer[pImageInfo->uOffset];
		CWOZHelper::TRKv2* pTRK = &pTRKS[indexFromTMAP];
		*pBitCount = pTRK->bitCount;
		*pNibbles = (pTRK->bitCount+7) / 8;

		const UINT maxNibblesPerTrack = pImageInfo->maxNibblesPerTrack;
		if (*pNibbles > (int)maxNibblesPerTrack)
		{
			_ASSERT(0);
			LogFileOutput("WOZ2 Read Track: attempting to read more than max nibbles! (phase=%f)\n", phase);
			return ReadEmptyTrack(pTrackImageBuffer, pNibbles, pBitCount);	// TODO: Enlarge track buffer, but for now just return an empty track
		}

		memcpy(pTrackImageBuffer, &pImageInfo->pImageBuffer[pTRK->startBlock*CWOZHelper::BLOCK_SIZE], *pNibbles);
	}

	// TODO: support writing a bitCount (ie. fractional nibbles)
	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles)
	{
		UINT hdrExtendedSize = 0;
		UINT trkExtendedSize = nNibbles;
		BYTE* pTrackMap = ((CWOZHelper::Tmap*)pImageInfo->pWOZTrackMap)->tmap;

		BYTE indexFromTMAP = pTrackMap[(BYTE)(phase * 2)];
		if (indexFromTMAP == CWOZHelper::TMAP_TRACK_EMPTY)
		{
			const BYTE track = (BYTE)(phase*2);
			{
				int highestIdx = -1;
				for (UINT i=0; i<CWOZHelper::MAX_QUARTER_TRACKS_5_25; i++)
				{
					if (pTrackMap[i] != CWOZHelper::TMAP_TRACK_EMPTY && pTrackMap[i] > highestIdx)
						highestIdx = pTrackMap[i];
				}
				indexFromTMAP = (highestIdx == -1) ? 0 : highestIdx+1;
			}
			pTrackMap[track] = indexFromTMAP;
			if (track-1 >= 0) pTrackMap[track-1] = indexFromTMAP;	// WOZ spec: track is also visible from neighboring quarter tracks
			if (track+1 < CWOZHelper::MAX_QUARTER_TRACKS_5_25)	pTrackMap[track+1] = indexFromTMAP;

			trkExtendedSize = (nNibbles + CWOZHelper::BLOCK_SIZE-1) & ~(CWOZHelper::BLOCK_SIZE-1);
			const UINT newImageSize = pImageInfo->uImageSize + trkExtendedSize;
			BYTE* pNewImageBuffer = new BYTE[newImageSize];

			memcpy(pNewImageBuffer, pImageInfo->pImageBuffer, pImageInfo->uImageSize);
			memset(pNewImageBuffer+pImageInfo->uImageSize, 0, trkExtendedSize);

			// NB. delete old pImageBuffer: pWOZTrackMap updated in WOZUpdateInfo() by parent function

			delete [] pImageInfo->pImageBuffer;
			pTrackMap = NULL;	// invalidate
			pImageInfo->pImageBuffer = pNewImageBuffer;
			pImageInfo->uImageSize = newImageSize;

			CWOZHelper::TRKv2* pTRKS = (CWOZHelper::TRKv2*) &pImageInfo->pImageBuffer[pImageInfo->uOffset];
			CWOZHelper::TRKv2* pTRK = &pTRKS[indexFromTMAP];
			pTRK->blockCount = trkExtendedSize / CWOZHelper::BLOCK_SIZE;
			pTRK->startBlock = 3;
			for (UINT i=0; i<indexFromTMAP; i++)
				pTRK->startBlock += pTRKS[i].blockCount;
			pTRK->bitCount = nNibbles * 8;

			CWOZHelper::WOZChunkHdr* pTrksHdr = (CWOZHelper::WOZChunkHdr*) (&pImageInfo->pImageBuffer[pImageInfo->uOffset] - sizeof(CWOZHelper::WOZChunkHdr));
			pTrksHdr->size += trkExtendedSize;

			hdrExtendedSize = ((BYTE*)pTRKS + sizeof(CWOZHelper::Trks) - pNewImageBuffer) - sizeof(CWOZHelper::WOZHeader);
		}

		CWOZHelper::TRKv2* pTRKS = (CWOZHelper::TRKv2*) &pImageInfo->pImageBuffer[pImageInfo->uOffset];
		CWOZHelper::TRKv2* pTRK = &pTRKS[indexFromTMAP];
		{
			UINT bitCount = pTRK->bitCount;
			UINT trackSize = (pTRK->bitCount + 7) / 8;
			_ASSERT(trackSize == nNibbles);
			if (trackSize != nNibbles)
			{
				_ASSERT(0);
				LogFileOutput("WOZ2 Write Track: attempting to write %08X when trackSize is %08X (phase=%f)\n", nNibbles, trackSize, phase);
				return;
			}
		}

		const long offset = pTRK->startBlock * CWOZHelper::BLOCK_SIZE;
		memcpy(&pImageInfo->pImageBuffer[offset], pTrackImageBuffer, nNibbles);

		if (!WriteImageData(pImageInfo, &pImageInfo->pImageBuffer[offset], trkExtendedSize, offset))
		{
			_ASSERT(0);
			LogFileOutput("WOZ2 Write Track: failed to write track (phase=%f) for file: %s\n", phase, pImageInfo->szFilename.c_str());
			return;
		}

		// TODO: zip/gzip: combine the track & hdr writes so that the file is only compressed & written once
		if (!UpdateWOZHeaderCRC(pImageInfo, this, hdrExtendedSize))
		{
			_ASSERT(0);
			LogFileOutput("WOZ2 Write Track: failed to write header CRC for file: %s\n", pImageInfo->szFilename.c_str());
		}
	}

	virtual bool AllowCreate(void) { return true; }
	virtual UINT GetImageSizeForCreate(void) { m_uNumTracksInImage = CWOZHelper::MAX_TRACKS_5_25; return sizeof(CWOZHelper::WOZHeader); }

	virtual eImageType GetType(void) { return eImageWOZ2; }
	virtual const char* GetCreateExtensions(void) { return ".woz"; }
	virtual const char* GetRejectExtensions(void) { return ".do;.dsk;.nib;.iie;.po;.prg"; }
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

//-----------------------------------------------------------------------------

eDetectResult C2IMGHelper::DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset)
{
	Header2IMG* pHdr = (Header2IMG*) pImage;

	if (dwImageSize < sizeof(Header2IMG) || pHdr->FormatID != FormatID_2IMG || pHdr->HeaderSize != sizeof(Header2IMG))
		return eMismatch;

	// https://github.com/AppleWin/AppleWin/issues/317
	// Work around some lazy implementations of the spec that set this value to 0 instead of the correct 1.
	if (pHdr->Version > 1)
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
			if (pHdr->DiskDataLength != TRACKS_STANDARD*NIBBLES_PER_TRACK_NIB)
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
		return DEFAULT_VOLUME_NUMBER;

	return m_Hdr.Flags.VolumeNumber;
}

bool C2IMGHelper::IsLocked(void)
{
	return m_Hdr.Flags.bDiskImageLocked;
}

//-----------------------------------------------------------------------------

// Pre: already matched the WOZ header
eDetectResult CWOZHelper::ProcessChunks(ImageInfo* pImageInfo, DWORD& dwOffset)
{
	UINT32* pImage32 = (uint32_t*) (pImageInfo->pImageBuffer + sizeof(WOZHeader));
	int imageSizeRemaining = pImageInfo->uImageSize - sizeof(WOZHeader);
	_ASSERT(imageSizeRemaining >= 0);
	if (imageSizeRemaining < 0)
		return eMismatch;

	while(imageSizeRemaining >= sizeof(WOZChunkHdr))
	{
		UINT32 chunkId = *pImage32++;
		UINT32 chunkSize = *pImage32++;
		imageSizeRemaining -= sizeof(WOZChunkHdr);

		switch(chunkId)
		{
			case INFO_CHUNK_ID:
				m_pInfo = (InfoChunkv2*)pImage32;
				if (m_pInfo->v1.diskType != InfoChunk::diskType5_25)
					return eMismatch;
				break;
			case TMAP_CHUNK_ID:
				pImageInfo->pWOZTrackMap = (BYTE*) pImage32;
				break;
			case TRKS_CHUNK_ID:
				dwOffset = pImageInfo->uOffset = pImageInfo->uImageSize - imageSizeRemaining;	// offset into image of track data
				break;
			case WRIT_CHUNK_ID:	// WOZ v2 (optional)
				break;
			case META_CHUNK_ID:	// (optional)
				break;
			default:	// no idea what this chunk is, so skip it
				_ASSERT(0);
				break;
		}

		pImage32 = (UINT32*) ((BYTE*)pImage32 + chunkSize);
		imageSizeRemaining -= chunkSize;
		_ASSERT(imageSizeRemaining >= 0);
		if (imageSizeRemaining < 0)
			return eMismatch;
	}

	return eMatch;
}

//-----------------------------------------------------------------------------

// NB. Of the 6 cases (floppy/harddisk x gzip/zip/normal) only harddisk-normal isn't read entirely to memory
// - harddisk-normal-create also doesn't create a max size image-buffer

// DETERMINE THE FILE'S EXTENSION AND CONVERT IT TO LOWERCASE
void CImageHelperBase::GetCharLowerExt(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize)
{
	LPCTSTR pImageFileExt = pszImageFilename;

	if (_tcsrchr(pImageFileExt, TEXT('\\')))
		pImageFileExt = _tcsrchr(pImageFileExt, TEXT('\\'))+1;

	if (_tcsrchr(pImageFileExt, TEXT('.')))
		pImageFileExt = _tcsrchr(pImageFileExt, TEXT('.'));

	_tcsncpy(pszExt, pImageFileExt, uExtSize);
	pszExt[uExtSize - 1] = 0;

	CharLowerBuff(pszExt, _tcslen(pszExt));
}

void CImageHelperBase::GetCharLowerExt2(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize)
{
	TCHAR szFilename[MAX_PATH];
	_tcsncpy(szFilename, pszImageFilename, MAX_PATH);
	szFilename[MAX_PATH - 1] = 0;

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

	int nLen = gzread(hGZFile, pImageInfo->pImageBuffer, MAX_UNCOMPRESSED_SIZE);
	int nRes = gzclose(hGZFile);	// close before returning (due to error) to avoid resource leak
	hGZFile = NULL;

	if (nLen < 0 || nLen == MAX_UNCOMPRESSED_SIZE)
		return eIMAGE_ERROR_BAD_SIZE;

	if (nRes != Z_OK)
		return eIMAGE_ERROR_GZ;

	//

	// Strip .gz then try to determine the file's extension and convert it to lowercase
	TCHAR szExt[_MAX_EXT] = "";
	GetCharLowerExt2(szExt, pszImageFilename, _MAX_EXT);

	DWORD dwSize = nLen;
	DWORD dwOffset = 0;
	CImageBase* pImageType = Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset, pImageInfo);

	if (!pImageType)
		return eIMAGE_ERROR_UNSUPPORTED;

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	SetImageInfo(pImageInfo, eFileGZip, dwOffset, pImageType, dwSize);
	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

ImageError_e CImageHelperBase::CheckZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, std::string& strFilenameInZip)
{
	unzFile hZipFile = unzOpen(pszImageFilename);
	if (hZipFile == NULL)
		return eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP;

	unz_global_info global_info;
	unz_file_info file_info;
	char szFilename[MAX_PATH];
	memset(szFilename, 0, sizeof(szFilename));
	BYTE* pImageBuffer = NULL;
	ImageInfo* pImageInfo2 = NULL;
	CImageBase* pImageType = NULL;
	UINT numValidImages = 0;

	try
	{
		int nRes = unzGetGlobalInfo(hZipFile, &global_info);
		if (nRes != UNZ_OK)
			throw eIMAGE_ERROR_ZIP;

		nRes = unzGoToFirstFile(hZipFile);
		if (nRes != UNZ_OK)
			throw eIMAGE_ERROR_ZIP;

		for (UINT n=0; n<global_info.number_entry; n++)
		{
			if (n)
			{
				nRes = unzGoToNextFile(hZipFile);
				if (nRes == UNZ_END_OF_LIST_OF_FILE)
					break;
				if (nRes != UNZ_OK)
					throw eIMAGE_ERROR_ZIP;
			}

			nRes = unzGetCurrentFileInfo(hZipFile, &file_info, szFilename, MAX_PATH, NULL, 0, NULL, 0);
			if (nRes != UNZ_OK)
				throw eIMAGE_ERROR_ZIP;

			const UINT uFileSize = file_info.uncompressed_size;
			if (uFileSize > GetMaxImageSize())
				throw eIMAGE_ERROR_BAD_SIZE;

			if (uFileSize == 0)	// skip directories or empty files
				continue;

			//

			nRes = unzOpenCurrentFile(hZipFile);
			if (nRes != UNZ_OK)
				throw eIMAGE_ERROR_ZIP;

			BYTE* pImageBuffer = new BYTE[uFileSize];
			int nLen = unzReadCurrentFile(hZipFile, pImageBuffer, uFileSize);
			if (nLen < 0)
			{
				unzCloseCurrentFile(hZipFile);	// Must CloseCurrentFile before Close
				throw eIMAGE_ERROR_UNSUPPORTED;
			}

			nRes = unzCloseCurrentFile(hZipFile);
			if (nRes != UNZ_OK)
				throw eIMAGE_ERROR_ZIP;

			// Determine the file's extension and convert it to lowercase
			TCHAR szExt[_MAX_EXT] = "";
			GetCharLowerExt(szExt, szFilename, _MAX_EXT);

			DWORD dwSize = nLen;
			DWORD dwOffset = 0;

			ImageInfo*& pImageInfoForDetect = !pImageInfo2 ? pImageInfo : pImageInfo2;
			pImageInfoForDetect->pImageBuffer = pImageBuffer;
			CImageBase* pNewImageType = Detect(pImageBuffer, dwSize, szExt, dwOffset, pImageInfoForDetect);

			if (pNewImageType)
			{
				numValidImages++;

				if (numValidImages == 1)
				{
					pImageType = pNewImageType;

					pImageInfo->szFilenameInZip = szFilename;
					memcpy(&pImageInfo->zipFileInfo.tmz_date, &file_info.tmu_date, sizeof(file_info.tmu_date));
					pImageInfo->zipFileInfo.dosDate     = file_info.dosDate;
					pImageInfo->zipFileInfo.internal_fa = file_info.internal_fa;
					pImageInfo->zipFileInfo.external_fa = file_info.external_fa;
					pImageInfo->uNumEntriesInZip = global_info.number_entry;
					pImageInfo->pImageBuffer = pImageBuffer;

					pImageBuffer = NULL;
					strFilenameInZip = szFilename;

					SetImageInfo(pImageInfo, eFileZip, dwOffset, pImageType, dwSize);

					pImageInfo2 = new ImageInfo();	// use this dummy one, as some members get overwritten during Detect()
				}
			}

			delete [] pImageBuffer;
			pImageBuffer = NULL;
		}
	}
	catch (ImageError_e error)
	{
		if (hZipFile)
			unzClose(hZipFile);

		delete [] pImageBuffer;
		delete pImageInfo2;

		return error;
	}

	delete pImageInfo2;

	int nRes = unzClose(hZipFile);
	hZipFile = NULL;
	if (nRes != UNZ_OK)
		return eIMAGE_ERROR_ZIP;

	//

	if (!pImageType)
		return eIMAGE_ERROR_UNSUPPORTED;

	const eImageType Type = pImageType->GetType();
	if (Type == eImageAPL || Type == eImageIIE || Type == eImagePRG)
		return eIMAGE_ERROR_UNSUPPORTED;

	if (global_info.number_entry > 1)
		pImageInfo->bWriteProtected = 1;	// Zip archives with multiple files are read-only (for now) - see WriteImageData() for zipfile

	pImageInfo->uNumValidImagesInZip = numValidImages;

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
			pImageInfo->bWriteProtected = true;
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

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(hFile, pImageInfo->pImageBuffer, dwSize, &dwBytesRead, NULL);
		if (!bRes || dwSize != dwBytesRead)
		{
			delete [] pImageInfo->pImageBuffer;
			pImageInfo->pImageBuffer = NULL;
			return eIMAGE_ERROR_BAD_SIZE;
		}

		pImageType = Detect(pImageInfo->pImageBuffer, dwSize, szExt, dwOffset, pImageInfo);
		if (bTempDetectBuffer)
		{
			delete [] pImageInfo->pImageBuffer;
			pImageInfo->pImageBuffer = NULL;
		}
	}
	else	// Create (or pre-existing zero-length file)
	{
		if (pImageInfo->bWriteProtected)
			return eIMAGE_ERROR_ZEROLENGTH_WRITEPROTECTED;	// Can't be formatted, so return error

		pImageType = GetImageForCreation(szExt, &dwSize);
		if (pImageType && dwSize)
		{
			if (pImageType->GetType() == eImageWOZ2)
			{
				pImageInfo->pImageBuffer = m_WOZHelper.CreateEmptyDisk(dwSize);
				pImageInfo->uImageSize = dwSize;
				bool res = WOZUpdateInfo(pImageInfo, dwOffset);
				_ASSERT(res);
			}
			else
			{
				pImageInfo->pImageBuffer = new BYTE[dwSize];

				if (pImageType->GetType() == eImageNIB1)
				{
					// Fill zero-length image buffer with alternating high-bit-set nibbles (GH#196, GH#338)
					for (UINT i=0; i<dwSize; i+=2)
					{
						pImageInfo->pImageBuffer[i+0] = 0x80;	// bit7 set, but 0x80 is an invalid nibble
						pImageInfo->pImageBuffer[i+1] = 0x81;	// bit7 set, but 0x81 is an invalid nibble
					}
				}
				else
				{
					memset(pImageInfo->pImageBuffer, 0, dwSize);
				}
			}

			// As a convenience, resize the file to the complete size (GH#506)
			// - this also means that a save-state done mid-way through a format won't reference an image file with a partial size (GH#494)
			DWORD dwBytesWritten = 0;
			BOOL res = WriteFile(hFile, pImageInfo->pImageBuffer, dwSize, &dwBytesWritten, NULL); 
			if (!res || dwBytesWritten != dwSize)
				return eIMAGE_ERROR_FAILED_TO_INIT_ZEROLENGTH;
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

	SetImageInfo(pImageInfo, eFileNormal, dwOffset, pImageType, dwSize);
	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

void CImageHelperBase::SetImageInfo(ImageInfo* pImageInfo, FileType_e fileType, DWORD dwOffset, CImageBase* pImageType, DWORD dwSize)
{
	pImageInfo->FileType = fileType;
	pImageInfo->uOffset = dwOffset;
	pImageInfo->pImageType = pImageType;
	pImageInfo->uImageSize = dwSize;
	pImageInfo->uNumTracks = pImageType->m_uNumTracksInImage;// Copy ImageType's m_uNumTracksInImage, which may get trashed by subsequent images in the zip (GH#824)
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

    if (uStrLen > GZ_SUFFIX_LEN && _stricmp(pszImageFilename+uStrLen-GZ_SUFFIX_LEN, GZ_SUFFIX) == 0)
	{
		Err = CheckGZipFile(pszImageFilename, pImageInfo);
	}
    else if (uStrLen > ZIP_SUFFIX_LEN && _stricmp(pszImageFilename+uStrLen-ZIP_SUFFIX_LEN, ZIP_SUFFIX) == 0)
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

	TCHAR szFilename[MAX_PATH] = { 0 };
	DWORD uNameLen = GetFullPathName(pszImageFilename, MAX_PATH, szFilename, NULL);
	pImageInfo->szFilename = szFilename;
	if (uNameLen == 0 || uNameLen >= MAX_PATH)
		Err = eIMAGE_ERROR_FAILED_TO_GET_PATHNAME;

	return eIMAGE_ERROR_NONE;
}

//-------------------------------------

void CImageHelperBase::Close(ImageInfo* pImageInfo)
{
	if (pImageInfo->hFile != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pImageInfo->hFile);
		pImageInfo->hFile = INVALID_HANDLE_VALUE;
	}

	pImageInfo->szFilename.clear();

	delete [] pImageInfo->pImageBuffer;
	pImageInfo->pImageBuffer = NULL;
}

//-------------------------------------

bool CImageHelperBase::WOZUpdateInfo(ImageInfo* pImageInfo, DWORD& dwOffset)
{
	if (m_WOZHelper.ProcessChunks(pImageInfo, dwOffset) != eMatch)
	{
		_ASSERT(0);
		return false;
	}

	if (m_WOZHelper.IsWriteProtected())
		pImageInfo->bWriteProtected = true;

	pImageInfo->optimalBitTiming = m_WOZHelper.GetOptimalBitTiming();
	pImageInfo->maxNibblesPerTrack = m_WOZHelper.GetMaxNibblesPerTrack();
	pImageInfo->bootSectorFormat = m_WOZHelper.GetBootSectorFormat();

	m_WOZHelper.InvalidateInfo();
	return true;
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
	m_vecImageTypes.push_back( new CWOZ1Image );
	m_vecImageTypes.push_back( new CWOZ2Image );
}

CImageBase* CDiskImageHelper::Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, ImageInfo* pImageInfo)
{
	dwOffset = 0;
	m_MacBinaryHelper.DetectHdr(pImage, dwSize, dwOffset);
	m_Result2IMG = m_2IMGHelper.DetectHdr(pImage, dwSize, dwOffset);
	pImageInfo->maxNibblesPerTrack = NIBBLES_PER_TRACK;	// Start with the default size (for all types). May get changed below.

	// CALL THE DETECTION FUNCTIONS IN ORDER, LOOKING FOR A MATCH
	eImageType imageType = eImageUNKNOWN;
	eImageType possibleType = eImageUNKNOWN;

	if (m_Result2IMG == eMatch)
	{
		if (m_2IMGHelper.IsImageFormatDOS33())
			imageType = eImageDO;
		else if (m_2IMGHelper.IsImageFormatProDOS())
			imageType = eImagePO;

		if (imageType != eImageUNKNOWN)
		{
			CImageBase* pImageType = GetImage(imageType);
			if (!pImageType || !pImageType->IsValidImageSize(dwSize))
				imageType = eImageUNKNOWN;
		}
	}

	if (imageType == eImageUNKNOWN)
	{
		for (UINT uLoop=0; uLoop < GetNumImages() && imageType == eImageUNKNOWN; uLoop++)
		{
			if (*pszExt && _tcsstr(GetImage(uLoop)->GetRejectExtensions(), pszExt))
				continue;

			eDetectResult Result = GetImage(uLoop)->Detect(pImage, dwSize, pszExt);
			if (Result == eMatch)
				imageType = GetImage(uLoop)->GetType();
			else if ((Result == ePossibleMatch) && (possibleType == eImageUNKNOWN))
				possibleType = GetImage(uLoop)->GetType();
		}
	}

	if (imageType == eImageUNKNOWN)
		imageType = possibleType;

	CImageBase* pImageType = GetImage(imageType);
	if (!pImageType)
		return NULL;

	if (imageType == eImageWOZ1 || imageType == eImageWOZ2)
	{
		CWOZHelper::WOZHeader* pWozHdr = (CWOZHelper::WOZHeader*) pImage;
		if (pWozHdr->crc32 && // WOZ spec: CRC of 0 should be ignored
			pWozHdr->crc32 != crc32(0, pImage+sizeof(CWOZHelper::WOZHeader), dwSize-sizeof(CWOZHelper::WOZHeader)))
		{
			int res = MessageBox(GetDesktopWindow(), "CRC mismatch\nContinue using image?", "AppleWin: WOZ Header", MB_ICONSTOP | MB_SETFOREGROUND | MB_YESNO);
			if (res == IDNO)
				return NULL;
		}

		pImageInfo->uImageSize = dwSize;
		if (!WOZUpdateInfo(pImageInfo, dwOffset))
			return NULL;
	}
	else
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
			pImageType->SetVolumeNumber( m_2IMGHelper.GetVolumeNumber() );

			if (m_2IMGHelper.IsLocked())
				pImageInfo->bWriteProtected = true;
		}
		else
		{
			pImageType->SetVolumeNumber(DEFAULT_VOLUME_NUMBER);
		}
	}

	return pImageType;
}

CImageBase* CDiskImageHelper::GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize)
{
	// WE CREATE ONLY DOS ORDER (DO), 6656-NIBBLE (NIB) OR WOZ2 (WOZ) FORMAT FILES
	for (UINT uLoop = 0; uLoop < GetNumImages(); uLoop++)
	{
		if (!GetImage(uLoop)->AllowCreate())
			continue;

		if (*pszExt && _tcsstr(GetImage(uLoop)->GetCreateExtensions(), pszExt))
		{
			CImageBase* pImageType = GetImage(uLoop);

			*pCreateImageSize = pImageType->GetImageSizeForCreate();	// Also sets m_uNumTracksInImage
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

CImageBase* CHardDiskImageHelper::Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, ImageInfo* pImageInfo)
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
			if (m_2IMGHelper.IsLocked())
				pImageInfo->bWriteProtected = true;
		}
	}

	pImageInfo->pWOZTrackMap = 0;	// TODO: WOZ
	pImageInfo->optimalBitTiming = 0;	// TODO: WOZ
	pImageInfo->maxNibblesPerTrack = 0;	// TODO

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

//-----------------------------------------------------------------------------

#define ASSERT_OFFSET(x, offset) _ASSERT( ((BYTE*)&pWOZ->x - (BYTE*)pWOZ) == offset )

BYTE* CWOZHelper::CreateEmptyDisk(DWORD& size)
{
	WOZEmptyImage525* pWOZ = new WOZEmptyImage525;
	memset(pWOZ, 0, sizeof(WOZEmptyImage525));
	size = sizeof(WOZEmptyImage525);
	_ASSERT(size == 3*BLOCK_SIZE);

	pWOZ->hdr.id1 = ID1_WOZ2;
	pWOZ->hdr.id2 = ID2;
	// hdr.crc32 done at end

	// INFO
	ASSERT_OFFSET(infoHdr, 12);
	pWOZ->infoHdr.id = INFO_CHUNK_ID;
	pWOZ->infoHdr.size = (BYTE*)&pWOZ->tmapHdr - (BYTE*)&pWOZ->info;
	_ASSERT(pWOZ->infoHdr.size == INFO_CHUNK_SIZE);
	pWOZ->info.v1.version = 2;
	pWOZ->info.v1.diskType = InfoChunk::diskType5_25;
	pWOZ->info.v1.cleaned = 1;
	std::string creator("AppleWin v");
	creator += std::string(VERSIONSTRING);
	memset(&pWOZ->info.v1.creator[0], ' ', sizeof(pWOZ->info.v1.creator));
	memcpy(&pWOZ->info.v1.creator[0], creator.c_str(), creator.size());	// don't include null
	pWOZ->info.diskSides = 1;
	pWOZ->info.bootSectorFormat = bootUnknown;	// could be INIT'd to 13 or 16 sector
	pWOZ->info.optimalBitTiming = InfoChunkv2::optimalBitTiming5_25;
	pWOZ->info.compatibleHardware = 0;	// unknown
	pWOZ->info.requiredRAM = 0;			// unknown
	pWOZ->info.largestTrack = TRK_DEFAULT_BLOCK_COUNT_5_25;		// unknown - but use default

	// TMAP
	ASSERT_OFFSET(tmapHdr, 80);
	pWOZ->tmapHdr.id = TMAP_CHUNK_ID;
	pWOZ->tmapHdr.size = sizeof(pWOZ->tmap);
	memset(&pWOZ->tmap, TMAP_TRACK_EMPTY, sizeof(pWOZ->tmap));	// all tracks empty

	// TRKS
	ASSERT_OFFSET(trksHdr, 248);
	pWOZ->trksHdr.id = TRKS_CHUNK_ID;
	pWOZ->trksHdr.size = sizeof(pWOZ->trks);
	for (UINT i = 0; i < sizeof(pWOZ->trks.trks) / sizeof(pWOZ->trks.trks[0]); i++)
	{
		pWOZ->trks.trks[i].startBlock = 0;	// minimum startBlock (at end of file!)
		pWOZ->trks.trks[i].blockCount = 0;
		pWOZ->trks.trks[i].bitCount = 0;
	}

	pWOZ->hdr.crc32 = crc32(0, (BYTE*)&pWOZ->infoHdr, sizeof(WOZEmptyImage525) - sizeof(WOZHeader));
	return (BYTE*) pWOZ;
}

#if _DEBUG
// Replace the call in CheckNormalFile() to CreateEmptyDiskv1() to generate a WOZv1 empty image-file
BYTE* CWOZHelper::CreateEmptyDiskv1(DWORD& size)
{
	WOZv1EmptyImage525* pWOZ = new WOZv1EmptyImage525;
	memset(pWOZ, 0, sizeof(WOZv1EmptyImage525));
	size = sizeof(WOZv1EmptyImage525);
	_ASSERT(size == 256);

	pWOZ->hdr.id1 = ID1_WOZ1;
	pWOZ->hdr.id2 = ID2;
	// hdr.crc32 done at end

	// INFO
	ASSERT_OFFSET(infoHdr, 12);
	pWOZ->infoHdr.id = INFO_CHUNK_ID;
	pWOZ->infoHdr.size = (BYTE*)&pWOZ->tmapHdr - (BYTE*)&pWOZ->info;
	_ASSERT(pWOZ->infoHdr.size == INFO_CHUNK_SIZE);
	pWOZ->info.version = 1;
	pWOZ->info.diskType = InfoChunk::diskType5_25;
	pWOZ->info.cleaned = 1;
	std::string creator("AppleWin v");
	creator += std::string(VERSIONSTRING);
	memset(&pWOZ->info.creator[0], ' ', sizeof(pWOZ->info.creator));
	memcpy(&pWOZ->info.creator[0], creator.c_str(), creator.size());	// don't include null

	// TMAP
	ASSERT_OFFSET(tmapHdr, 80);
	pWOZ->tmapHdr.id = TMAP_CHUNK_ID;
	pWOZ->tmapHdr.size = sizeof(pWOZ->tmap);
	memset(&pWOZ->tmap, TMAP_TRACK_EMPTY, sizeof(pWOZ->tmap));	// all tracks empty

	// TRKS
	ASSERT_OFFSET(trksHdr, 248);
	pWOZ->trksHdr.id = TRKS_CHUNK_ID;
	pWOZ->trksHdr.size = 0;

	pWOZ->hdr.crc32 = crc32(0, (BYTE*)&pWOZ->infoHdr, sizeof(WOZv1EmptyImage525) - sizeof(WOZHeader));
	return (BYTE*) pWOZ;
}
#endif
