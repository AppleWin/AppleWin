#pragma once

#define NIBBLES_PER_TRACK 0x1A00
#define TRACK_DENIBBLIZED_SIZE (16 * 256)	// #Sectors x Sector-size

#define	TRACKS_STANDARD	35
#define	TRACKS_EXTRA	5		// Allow up to a 40-track .dsk image (160KB)
#define	TRACKS_MAX		(TRACKS_STANDARD+TRACKS_EXTRA)

enum ImageError_e
{
	eIMAGE_ERROR_NONE,
	eIMAGE_ERROR_BAD_POINTER,
	eIMAGE_ERROR_BAD_SIZE,
	eIMAGE_ERROR_BAD_FILE,
	eIMAGE_ERROR_UNSUPPORTED,
	eIMAGE_ERROR_UNSUPPORTED_HDV,
	eIMAGE_ERROR_GZ,
	eIMAGE_ERROR_ZIP,
	eIMAGE_ERROR_REJECTED_MULTI_ZIP,
	eIMAGE_ERROR_UNSUPPORTED_MULTI_ZIP,
	eIMAGE_ERROR_UNABLE_TO_OPEN,
	eIMAGE_ERROR_UNABLE_TO_OPEN_GZ,
	eIMAGE_ERROR_UNABLE_TO_OPEN_ZIP,
};

ImageError_e ImageOpen(LPCTSTR pszImageFilename, HIMAGE* hDiskImage_, bool* pWriteProtected_, const bool bCreateIfNecessary, std::string& strFilenameInZip);
void ImageClose(const HIMAGE hDiskImage, const bool bOpenError=false);
BOOL ImageBoot(const HIMAGE hDiskImage);
void ImageDestroy(void);
void ImageInitialize(void);

void ImageReadTrack(const HIMAGE hDiskImage, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles);
void ImageWriteTrack(const HIMAGE hDiskImage, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles);

int ImageGetNumTracks(const HIMAGE hDiskImage);
bool ImageIsWriteProtected(const HIMAGE hDiskImage);
bool ImageIsMultiFileZip(const HIMAGE hDiskImage);
