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

/* Description: Disk Image
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "DiskImage.h"
#include "Common.h"
#include "DiskImageHelper.h"


static CDiskImageHelper sg_DiskImageHelper;
static CHardDiskImageHelper sg_HardDiskImageHelper;

//===========================================================================

// Pre: *pWriteProtected_ already set to file's r/w status - see DiskInsert()
ImageError_e ImageOpen(	const std::string & pszImageFilename,
						ImageInfo** ppImageInfo,
						bool* pWriteProtected,
						const bool bCreateIfNecessary,
						std::string& strFilenameInZip,
						const bool bExpectFloppy /*=true*/)
{
	if (!(!pszImageFilename.empty() && ppImageInfo && pWriteProtected))
		return eIMAGE_ERROR_BAD_POINTER;

	// CREATE A RECORD FOR THE FILE
	*ppImageInfo = new ImageInfo();

	ImageInfo* pImageInfo = *ppImageInfo;
	pImageInfo->bWriteProtected = *pWriteProtected;
	if (bExpectFloppy)	pImageInfo->pImageHelper = &sg_DiskImageHelper;
	else				pImageInfo->pImageHelper = &sg_HardDiskImageHelper;

	ImageError_e Err = pImageInfo->pImageHelper->Open(pszImageFilename.c_str(), pImageInfo, bCreateIfNecessary, strFilenameInZip);
	if (Err != eIMAGE_ERROR_NONE)
	{
		ImageClose(*ppImageInfo);
		*ppImageInfo = NULL;
		return Err;
	}

	if (pImageInfo->pImageType && pImageInfo->pImageType->GetType() == eImageHDV)
	{
		if (bExpectFloppy)
			Err = eIMAGE_ERROR_UNSUPPORTED_HDV;
		return Err;
	}

	// THE FILE MATCHES A KNOWN FORMAT

	_ASSERT(bExpectFloppy);
	if (!bExpectFloppy)
		return eIMAGE_ERROR_UNSUPPORTED;

	_ASSERT(pImageInfo->uNumTracks);

	*pWriteProtected = pImageInfo->bWriteProtected;

	return eIMAGE_ERROR_NONE;
}

//===========================================================================

void ImageClose(ImageInfo* const pImageInfo)
{
	pImageInfo->pImageHelper->Close(pImageInfo);

	delete pImageInfo;
}

//===========================================================================

BOOL ImageBoot(ImageInfo* const pImageInfo)
{
	BOOL result = 0;

	if (pImageInfo->pImageType->AllowBoot())
		result = pImageInfo->pImageType->Boot(pImageInfo);

	if (result)
		pImageInfo->bWriteProtected = 1;

	return result;
}

//===========================================================================

void ImageReadTrack(	ImageInfo* const pImageInfo,
						float phase,			// phase [0..79] +/- 0.5
						LPBYTE pTrackImageBuffer,
						int* pNibbles,
						UINT* pBitCount,
						bool enhanceDisk)
{
	_ASSERT(phase >= 0);
	if (phase < 0)
		phase = 0;

	const UINT track = pImageInfo->pImageType->PhaseToTrack(phase);

	if (pImageInfo->pImageType->AllowRW())
	{
		pImageInfo->pImageType->Read(pImageInfo, phase, pTrackImageBuffer, pNibbles, pBitCount, enhanceDisk);
	}
	else
	{
		*pNibbles = (int) ImageGetMaxNibblesPerTrack(pImageInfo);
		for (int i = 0; i < *pNibbles; i++)
			pTrackImageBuffer[i] = (BYTE)(rand() & 0xFF);
	}
}

//===========================================================================

void ImageWriteTrack(	ImageInfo* const pImageInfo,
						float phase,			// phase [0..79] +/- 0.5
						LPBYTE pTrackImageBuffer,
						const int nNibbles)
{
	_ASSERT(phase >= 0);
	if (phase < 0)
		phase = 0;

	const UINT track = pImageInfo->pImageType->PhaseToTrack(phase);

	if (pImageInfo->pImageType->AllowRW() && !pImageInfo->bWriteProtected)
	{
		pImageInfo->pImageType->Write(pImageInfo, phase, pTrackImageBuffer, nNibbles);

		eImageType imageType = pImageInfo->pImageType->GetType();
		if (imageType == eImageWOZ1 || imageType == eImageWOZ2)
		{
			DWORD dummy;
			bool res = sg_DiskImageHelper.WOZUpdateInfo(pImageInfo, dummy);
			_ASSERT(res);
		}
	}
}

//===========================================================================

bool ImageReadBlock(	ImageInfo* const pImageInfo,
						UINT nBlock,
						LPBYTE pBlockBuffer)
{
	bool bRes = false;
	if (pImageInfo->pImageType->AllowRW())
		bRes = pImageInfo->pImageType->Read(pImageInfo, nBlock, pBlockBuffer);

	return bRes;
}

//===========================================================================

bool ImageWriteBlock(	ImageInfo* const pImageInfo,
						UINT nBlock,
						LPBYTE pBlockBuffer)
{
	bool bRes = false;
	if (pImageInfo->pImageType->AllowRW() && !pImageInfo->bWriteProtected)
		bRes = pImageInfo->pImageType->Write(pImageInfo, nBlock, pBlockBuffer);

	return bRes;
}

//===========================================================================

UINT ImageGetNumTracks(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->uNumTracks : 0;
}

bool ImageIsWriteProtected(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->bWriteProtected : true;
}

bool ImageIsMultiFileZip(ImageInfo* const pImageInfo)
{
	return pImageInfo ? (pImageInfo->uNumValidImagesInZip > 1) : false;
}

const std::string & ImageGetPathname(ImageInfo* const pImageInfo)
{
	static const std::string szEmpty;
	return pImageInfo ? pImageInfo->szFilename : szEmpty;
}

UINT ImageGetImageSize(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->uImageSize : 0;
}

bool ImageIsWOZ(ImageInfo* const pImageInfo)
{
	return pImageInfo ? (pImageInfo->pImageType->GetType() == eImageWOZ1 || pImageInfo->pImageType->GetType() == eImageWOZ2) : false;
}

BYTE ImageGetOptimalBitTiming(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->optimalBitTiming : 32;
}

bool ImageIsBootSectorFormatSector13(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->bootSectorFormat == CWOZHelper::bootSector13 : false;
}

UINT ImagePhaseToTrack(ImageInfo* const pImageInfo, const float phase, const bool limit/*=true*/)
{
	if (!pImageInfo)
		return 0;

	UINT track = pImageInfo->pImageType->PhaseToTrack(phase);

	if (limit)
	{
		const UINT numTracksInImage = ImageGetNumTracks(pImageInfo);
		track = (numTracksInImage == 0) ? 0
										: MIN(numTracksInImage - 1, track);
	}

	return track;
}

UINT ImageGetMaxNibblesPerTrack(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->maxNibblesPerTrack : NIBBLES_PER_TRACK;
}

void GetImageTitle(LPCTSTR pPathname, std::string & pImageName, std::string & pFullName)
{
	TCHAR   imagetitle[ MAX_DISK_FULL_NAME+1 ];
	LPCTSTR startpos = pPathname;

	// imagetitle = <FILENAME.EXT>
	if (_tcsrchr(startpos, TEXT('\\')))
		startpos = _tcsrchr(startpos, TEXT('\\'))+1;

	_tcsncpy(imagetitle, startpos, MAX_DISK_FULL_NAME);
	imagetitle[MAX_DISK_FULL_NAME] = 0;

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
		CharLowerBuff(imagetitle+1, _tcslen(imagetitle+1));

	// pFullName = <FILENAME.EXT>
	pFullName = imagetitle;

	if (imagetitle[0])
	{
		LPTSTR dot = imagetitle;
		if (_tcsrchr(dot, TEXT('.')))
			dot = _tcsrchr(dot, TEXT('.'));
		if (dot > imagetitle)
			*dot = 0;
	}

	// pImageName = <FILENAME> (ie. no extension)
	pImageName = imagetitle;
}
