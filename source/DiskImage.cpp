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
#pragma  hdrstop


static CDiskImageHelper sg_DiskImageHelper;

//===========================================================================

// Pre: *pWriteProtected_ already set to file's r/w status - see DiskInsert()
ImageError_e ImageOpen(	LPCTSTR pszImageFilename,
						HIMAGE* hDiskImage_,
						bool* pWriteProtected_,
						const bool bCreateIfNecessary,
						std::string& strFilenameInZip)
{
	if (! (pszImageFilename && hDiskImage_ && pWriteProtected_ && sg_DiskImageHelper.GetWorkBuffer()))
		return eIMAGE_ERROR_BAD_POINTER;

	// CREATE A RECORD FOR THE FILE, AND RETURN AN IMAGE HANDLE
	*hDiskImage_ = (HIMAGE) VirtualAlloc(NULL, sizeof(ImageInfo), MEM_COMMIT, PAGE_READWRITE);
	if (*hDiskImage_ == NULL)
		return eIMAGE_ERROR_BAD_POINTER;

	ZeroMemory(*hDiskImage_, sizeof(ImageInfo));
	ImageInfo* pImageInfo = (ImageInfo*) *hDiskImage_;
	pImageInfo->bWriteProtected = *pWriteProtected_;

	ImageError_e Err = sg_DiskImageHelper.Open(pszImageFilename, pImageInfo, bCreateIfNecessary, strFilenameInZip);

	if (pImageInfo->pImageType != NULL && Err == eIMAGE_ERROR_NONE && pImageInfo->pImageType->GetType() == eImageHDV)
		Err = eIMAGE_ERROR_UNSUPPORTED_HDV;

	if (Err != eIMAGE_ERROR_NONE)
	{
		ImageClose(*hDiskImage_, true);
		*hDiskImage_ = (HIMAGE)0;
		return Err;
	}

	// THE FILE MATCHES A KNOWN FORMAT

	pImageInfo->uNumTracks = sg_DiskImageHelper.GetNumTracksInImage(pImageInfo->pImageType);

	for (UINT uTrack = 0; uTrack < pImageInfo->uNumTracks; uTrack++)
		pImageInfo->ValidTrack[uTrack] = (pImageInfo->uImageSize > 0) ? 1 : 0;

	*pWriteProtected_ = pImageInfo->bWriteProtected;

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
				// What's the reason for this?
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
