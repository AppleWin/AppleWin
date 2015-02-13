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
#include "Common.h"

#include "DiskImage.h"
#include "DiskImageHelper.h"


static CDiskImageHelper sg_DiskImageHelper;

//===========================================================================

// Pre: *pWriteProtected_ already set to file's r/w status - see DiskInsert()
ImageError_e ImageOpen(	LPCTSTR pszImageFilename,
						HIMAGE* hDiskImage,
						bool* pWriteProtected,
						const bool bCreateIfNecessary,
						std::string& strFilenameInZip,
						const bool bExpectFloppy /*=true*/)
{
	if (! (pszImageFilename && hDiskImage && pWriteProtected && sg_DiskImageHelper.GetWorkBuffer()))
		return eIMAGE_ERROR_BAD_POINTER;

	// CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
	*hDiskImage = (HIMAGE) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
	if (*hDiskImage == NULL)
		return eIMAGE_ERROR_BAD_POINTER;

	ZeroMemory(*hDiskImage, sizeof(ImageInfo));
	ImageInfo* pImageInfo = (ImageInfo*) *hDiskImage;
	pImageInfo->bWriteProtected = *pWriteProtected;

	ImageError_e Err = sg_DiskImageHelper.Open(pszImageFilename, pImageInfo, bCreateIfNecessary, strFilenameInZip);

	if (Err != eIMAGE_ERROR_NONE)
	{
		ImageClose(*hDiskImage, true);
		*hDiskImage = (HIMAGE)0;
		return Err;
	}

	if (pImageInfo->pImageType && pImageInfo->pImageType->GetType() == eImageHDV)
	{
		if (bExpectFloppy)
			Err = eIMAGE_ERROR_UNSUPPORTED_HDV;
		return Err;
	}

	// THE FILE MATCHES A KNOWN FORMAT

	pImageInfo->uNumTracks = sg_DiskImageHelper.GetNumTracksInImage(pImageInfo->pImageType);

	for (UINT uTrack = 0; uTrack < pImageInfo->uNumTracks; uTrack++)
		pImageInfo->ValidTrack[uTrack] = (pImageInfo->uImageSize > 0) ? 1 : 0;

	*pWriteProtected = pImageInfo->bWriteProtected;

	return eIMAGE_ERROR_NONE;
}

//===========================================================================

void ImageClose(const HIMAGE hDiskImage, const bool bOpenError /*=false*/)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	bool bDeleteFile = false;

	if (!bOpenError)
	{
		for (UINT uTrack = 0; uTrack < ptr->uNumTracks; uTrack++)
		{
			if (!ptr->ValidTrack[uTrack])
			{
				// TODO: Comment using info from this URL:
				// http://groups.google.de/group/comp.emulators.apple2/msg/7a1b9317e7905152
				bDeleteFile = true;
				break;
			}
		}
	}

	sg_DiskImageHelper.Close(ptr, bDeleteFile);

	VirtualFree(ptr, 0, MEM_RELEASE);
}

//===========================================================================

BOOL ImageBoot(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	BOOL result = 0;

	if (ptr->pImageType->AllowBoot())
		result = ptr->pImageType->Boot(ptr);

	if (result)
		ptr->bWriteProtected = 1;

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

void ImageReadTrack(	const HIMAGE hDiskImage,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImageBuffer,
						int* pNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	if (ptr->pImageType->AllowRW() && ptr->ValidTrack[nTrack])
	{
		ptr->pImageType->Read(ptr, nTrack, nQuarterTrack, pTrackImageBuffer, pNibbles);
	}
	else
	{
		for (*pNibbles = 0; *pNibbles < NIBBLES_PER_TRACK; (*pNibbles)++)
			pTrackImageBuffer[*pNibbles] = (BYTE)(rand() & 0xFF);
	}
}

//===========================================================================

void ImageWriteTrack(	const HIMAGE hDiskImage,
						const int nTrack,
						const int nQuarterTrack,
						LPBYTE pTrackImage,
						const int nNibbles)
{
	_ASSERT(nTrack >= 0);
	if (nTrack < 0)
		return;

	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	if (ptr->pImageType->AllowRW() && !ptr->bWriteProtected)
	{
		ptr->pImageType->Write(ptr, nTrack, nQuarterTrack, pTrackImage, nNibbles);
		ptr->ValidTrack[nTrack] = 1;
	}
}

//===========================================================================

bool ImageReadBlock(	const HIMAGE hDiskImage,
						UINT nBlock,
						LPBYTE pBlockBuffer)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;

	bool bRes = false;
	if (ptr->pImageType->AllowRW())
		bRes = ptr->pImageType->Read(ptr, nBlock, pBlockBuffer);

	return bRes;
}

//===========================================================================

bool ImageWriteBlock(	const HIMAGE hDiskImage,
						UINT nBlock,
						LPBYTE pBlockBuffer)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;

	bool bRes = false;
	if (ptr->pImageType->AllowRW() && !ptr->bWriteProtected)
		bRes = ptr->pImageType->Write(ptr, nBlock, pBlockBuffer);

	return bRes;
}

//===========================================================================

int ImageGetNumTracks(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->uNumTracks : 0;
}

bool ImageIsWriteProtected(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->bWriteProtected : true;
}

bool ImageIsMultiFileZip(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? (ptr->uNumEntriesInZip > 1) : false;
}

const char* ImageGetPathname(const HIMAGE hDiskImage)
{
	static char* szEmpty = "";
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->szFilename : szEmpty;
}

UINT ImageGetImageSize(const HIMAGE hDiskImage)
{
	ImageInfo* ptr = (ImageInfo*) hDiskImage;
	return ptr ? ptr->uImageSize : 0;
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
