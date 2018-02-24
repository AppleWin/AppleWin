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
#include "DiskImageHelper.h"


static CDiskImageHelper sg_DiskImageHelper;
static CHardDiskImageHelper sg_HardDiskImageHelper;

//===========================================================================

// Pre: *pWriteProtected_ already set to file's r/w status - see DiskInsert()
ImageError_e ImageOpen(	LPCTSTR pszImageFilename,
						ImageInfo** ppImageInfo,
						bool* pWriteProtected,
						const bool bCreateIfNecessary,
						std::string& strFilenameInZip,
						const bool bExpectFloppy /*=true*/)
{
	if (bExpectFloppy && sg_DiskImageHelper.GetWorkBuffer() == NULL)
		return eIMAGE_ERROR_BAD_POINTER;

	if (! (pszImageFilename && ppImageInfo && pWriteProtected))
		return eIMAGE_ERROR_BAD_POINTER;

	// CREATE A RECORD FOR THE FILE
	*ppImageInfo = (ImageInfo*) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
	if (*ppImageInfo == NULL)
		return eIMAGE_ERROR_BAD_POINTER;

	ZeroMemory(*ppImageInfo, sizeof(ImageInfo));
	ImageInfo* pImageInfo = *ppImageInfo;
	pImageInfo->bWriteProtected = *pWriteProtected;
	if (bExpectFloppy)	pImageInfo->pImageHelper = &sg_DiskImageHelper;
	else				pImageInfo->pImageHelper = &sg_HardDiskImageHelper;

	ImageError_e Err = pImageInfo->pImageHelper->Open(pszImageFilename, pImageInfo, bCreateIfNecessary, strFilenameInZip);
	if (Err != eIMAGE_ERROR_NONE)
	{
		ImageClose(*ppImageInfo, true);
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

	pImageInfo->uNumTracks = sg_DiskImageHelper.GetNumTracksInImage(pImageInfo->pImageType);

	for (UINT uTrack = 0; uTrack < pImageInfo->uNumTracks; uTrack++)
		pImageInfo->ValidTrack[uTrack] = (pImageInfo->uImageSize > 0) ? 1 : 0;

	*pWriteProtected = pImageInfo->bWriteProtected;

	return eIMAGE_ERROR_NONE;
}

//===========================================================================

void ImageClose(ImageInfo* const pImageInfo, const bool bOpenError /*=false*/)
{
	bool bDeleteFile = false;

	if (!bOpenError)
	{
		for (UINT uTrack = 0; uTrack < pImageInfo->uNumTracks; uTrack++)
		{
			if (!pImageInfo->ValidTrack[uTrack])
			{
				// TODO: Comment using info from this URL:
				// http://groups.google.de/group/comp.emulators.apple2/msg/7a1b9317e7905152
				bDeleteFile = true;
				break;
			}
		}
	}

	pImageInfo->pImageHelper->Close(pImageInfo, bDeleteFile);

	VirtualFree(pImageInfo, 0, MEM_RELEASE);
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

void ImageDestroy(void)
{
	VirtualFree(sg_DiskImageHelper.GetWorkBuffer(), 0, MEM_RELEASE);
	sg_DiskImageHelper.SetWorkBuffer(NULL);
}

//===========================================================================

void ImageInitialize(void)
{
	LPBYTE pBuffer = (LPBYTE) VirtualAlloc(NULL, TRACK_DENIBBLIZED_SIZE*2, MEM_COMMIT, PAGE_READWRITE);
	sg_DiskImageHelper.SetWorkBuffer(pBuffer);
}

//===========================================================================

void ImageReadTrack(	ImageInfo* const pImageInfo,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImageBuffer,
						int* pNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	if (pImageInfo->pImageType->AllowRW() && pImageInfo->ValidTrack[nTrack])
	{
		pImageInfo->pImageType->Read(pImageInfo, nTrack, nQuarterTrack, pTrackImageBuffer, pNibbles);
	}
	else
	{
		for (*pNibbles = 0; *pNibbles < NIBBLES_PER_TRACK; (*pNibbles)++)
			pTrackImageBuffer[*pNibbles] = (BYTE)(rand() & 0xFF);
	}
}

//===========================================================================

void ImageWriteTrack(	ImageInfo* const pImageInfo,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImage,
						const int nNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	if (pImageInfo->pImageType->AllowRW() && !pImageInfo->bWriteProtected)
	{
		pImageInfo->pImageType->Write(pImageInfo, nTrack, nQuarterTrack, pTrackImage, nNibbles);
		pImageInfo->ValidTrack[nTrack] = 1;
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

int ImageGetNumTracks(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->uNumTracks : 0;
}

bool ImageIsWriteProtected(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->bWriteProtected : true;
}

bool ImageIsMultiFileZip(ImageInfo* const pImageInfo)
{
	return pImageInfo ? (pImageInfo->uNumEntriesInZip > 1) : false;
}

const char* ImageGetPathname(ImageInfo* const pImageInfo)
{
	static const char* szEmpty = "";
	return pImageInfo ? pImageInfo->szFilename : szEmpty;
}

UINT ImageGetImageSize(ImageInfo* const pImageInfo)
{
	return pImageInfo ? pImageInfo->uImageSize : 0;
}

void GetImageTitle(LPCTSTR pPathname, TCHAR* pImageName, TCHAR* pFullName)
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
	_tcsncpy( pFullName, imagetitle, MAX_DISK_FULL_NAME );
	pFullName[ MAX_DISK_FULL_NAME ] = 0;

	if (imagetitle[0])
	{
		LPTSTR dot = imagetitle;
		if (_tcsrchr(dot, TEXT('.')))
			dot = _tcsrchr(dot, TEXT('.'));
		if (dot > imagetitle)
			*dot = 0;
	}

	// pImageName = <FILENAME> (ie. no extension)
	_tcsncpy( pImageName, imagetitle, MAX_DISK_IMAGE_NAME );
	pImageName[ MAX_DISK_IMAGE_NAME ] = 0;
}
