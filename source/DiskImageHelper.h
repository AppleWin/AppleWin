#pragma once

#include "DiskDefs.h"
#include "DiskImage.h"
#include "zip.h"

#define GZ_SUFFIX ".gz"
#define GZ_SUFFIX_LEN (sizeof(GZ_SUFFIX)-1)

#define ZIP_SUFFIX ".zip"
#define ZIP_SUFFIX_LEN (sizeof(ZIP_SUFFIX)-1)


enum eImageType {eImageUNKNOWN, eImageDO, eImagePO, eImageNIB1, eImageNIB2, eImageHDV, eImageIIE, eImageAPL, eImagePRG, eImageWOZ1, eImageWOZ2};
enum eDetectResult {eMismatch, ePossibleMatch, eMatch};

class CImageBase;
class CImageHelperBase;

enum FileType_e {eFileNormal, eFileGZip, eFileZip};

struct ImageInfo
{
	std::string 	szFilename;
	CImageBase*		pImageType;
	CImageHelperBase* pImageHelper;
	FileType_e		FileType;
	HANDLE			hFile;
	DWORD			uOffset;
	bool			bWriteProtected;
	UINT			uImageSize;
	std::string		szFilenameInZip;
	zip_fileinfo	zipFileInfo;
	UINT			uNumEntriesInZip;
	UINT			uNumValidImagesInZip;
	// Floppy only
	UINT			uNumTracks;
	BYTE*			pImageBuffer;
	BYTE*			pWOZTrackMap;		// WOZ only (points into pImageBuffer)
	BYTE			optimalBitTiming;	// WOZ only
	BYTE			bootSectorFormat;	// WOZ only
	UINT			maxNibblesPerTrack;

	ImageInfo();
};

//-------------------------------------

#define HD_BLOCK_SIZE 512

#define UNIDISK35_800K_SIZE (800*1024)	// UniDisk 3.5"
#define HARDDISK_32M_SIZE (HD_BLOCK_SIZE * 65536)

#define DEFAULT_VOLUME_NUMBER 254

class CImageBase
{
public:
	CImageBase(void);
	virtual ~CImageBase(void);

	virtual bool Boot(ImageInfo* pImageInfo) { return false; }
	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt) = 0;
	virtual void Read(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int* pNibbles, UINT* pBitCount, bool enhanceDisk) { }
	virtual bool Read(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer) { return false; }
	virtual void Write(ImageInfo* pImageInfo, const float phase, LPBYTE pTrackImageBuffer, int nNibbles) { }
	virtual bool Write(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer) { return false; }

	virtual bool AllowBoot(void) { return false; }		// Only:    APL and PRG
	virtual bool AllowRW(void) { return true; }			// All but: APL and PRG
	virtual bool AllowCreate(void) { return false; }	// WE CREATE ONLY DOS ORDER (DO) OR 6656-NIBBLE (NIB) FORMAT FILES
	virtual UINT GetImageSizeForCreate(void) { _ASSERT(0); return (UINT)-1; }

	virtual eImageType GetType(void) = 0;
	virtual const char* GetCreateExtensions(void) = 0;
	virtual const char* GetRejectExtensions(void) = 0;

	bool WriteImageHeader(ImageInfo* pImageInfo, LPBYTE pHdr, const UINT hdrSize);
	void SetVolumeNumber(const BYTE uVolumeNumber) { m_uVolumeNumber = uVolumeNumber; }
	bool IsValidImageSize(const DWORD uImageSize);

	// To accurately convert a half phase (quarter track) back to a track (round half tracks down), use: ceil(phase)/2, eg:
	// . phase=4,+1 half phase = phase 4.5 => ceil(4.5)/2 = track 2 (OK)
	// . phase=4,-1 half phase = phase 3.5 => ceil(3.5)/2 = track 2 (OK)
	UINT PhaseToTrack(const float phase) { return ((UINT)ceil(phase)) >> 1; }

	enum SectorOrder_e {eProDOSOrder, eDOSOrder, eSIMSYSTEMOrder, NUM_SECTOR_ORDERS};

protected:
	bool ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);
	bool WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);
	bool ReadBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer);
	bool WriteBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer);
	bool WriteImageData(ImageInfo* pImageInfo, LPBYTE pSrcBuffer, const UINT uSrcSize, const long offset);

	LPBYTE Code62(int sector);
	void Decode62(LPBYTE imageptr);
	void DenibblizeTrack (LPBYTE trackimage, SectorOrder_e SectorOrder, int nibbles);
	DWORD NibblizeTrack (LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track);
	void SkewTrack (const int nTrack, const int nNumNibbles, const LPBYTE pTrackImageBuffer);

public:
	UINT m_uNumTracksInImage;	// Init'd by CDiskImageHelper.Detect()/GetImageForCreation() & possibly updated by IsValidImageSize()

protected:
	static BYTE ms_DiskByte[0x40];
	static BYTE ms_SectorNumber[NUM_SECTOR_ORDERS][NUM_SECTORS];
	BYTE m_uVolumeNumber;
	LPBYTE m_pWorkBuffer;
};

//-------------------------------------

class CHdrHelper
{
public:
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset) = 0;
	virtual UINT GetMaxHdrSize(void) = 0;
protected:
	CHdrHelper(void) {}
	virtual ~CHdrHelper(void) {}
};

class CMacBinaryHelper : public CHdrHelper
{
public:
	CMacBinaryHelper(void) {}
	virtual ~CMacBinaryHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset);
	virtual UINT GetMaxHdrSize(void) { return uMacBinHdrSize; }

private:
	static const UINT uMacBinHdrSize = 128;
};

// http://apple2.org.za/gswv/a2zine/Docs/DiskImage_2MG_Info.txt

#pragma pack(push)
#pragma pack(1)	// Ensure Header2IMG & WOZ structs are packed

#pragma warning(push)
#pragma warning(disable: 4200)	// Allow zero-sized array in struct


class C2IMGHelper : public CHdrHelper
{
public:
	C2IMGHelper(const bool bIsFloppy) : m_bIsFloppy(bIsFloppy) {}
	virtual ~C2IMGHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset);
	virtual UINT GetMaxHdrSize(void) { return sizeof(Header2IMG); }
	BYTE GetVolumeNumber(void);
	bool IsLocked(void);
	bool IsImageFormatDOS33(void) { return m_Hdr.ImageFormat == e2IMGFormatDOS33; }
	bool IsImageFormatProDOS(void) { return m_Hdr.ImageFormat == e2IMGFormatProDOS; }

private:
	static const UINT32 FormatID_2IMG = 'GMI2';			// '2IMG'
	static const UINT32 Creator_2IMG_AppleWin = '1vWA';	// 'AWv1'
	static const USHORT Version_2IMG_AppleWin = 1;

	enum ImageFormat2IMG_e { e2IMGFormatDOS33=0, e2IMGFormatProDOS, e2IMGFormatNIBData };

	struct Flags2IMG
	{
		UINT32 VolumeNumber : 8;				// bits7-0
		UINT32 bDOS33VolumeNumberValid : 1;
		UINT32 Pad : 22;
		UINT32 bDiskImageLocked : 1;			// bit31
	};

	struct Header2IMG
	{
		UINT32	FormatID;		// "2IMG"
		UINT32	CreatorID;
		USHORT	HeaderSize;
		USHORT	Version;
		union
		{
			ImageFormat2IMG_e	ImageFormat;
			UINT32				ImageFormatRaw;
		};
		union
		{
			Flags2IMG			Flags;
			UINT32				FlagsRaw;
		};
		UINT32	NumBlocks;		// The number of 512-byte blocks in the disk image 
		UINT32	DiskDataOffset;
		UINT32	DiskDataLength; 
		UINT32	CommentOffset;	// Optional
		UINT32	CommentLength;	// Optional
		UINT32	CreatorOffset;	// Optional
		UINT32	CreatorLength;	// Optional
		BYTE	Padding[16];
	};

	Header2IMG m_Hdr;
	bool m_bIsFloppy;
};

class CWOZHelper : public CHdrHelper
{
public:
	CWOZHelper() :
		m_pInfo(NULL)
	{}
	virtual ~CWOZHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset) { _ASSERT(0); return eMismatch; }
	virtual UINT GetMaxHdrSize(void) { return sizeof(WOZHeader); }
	eDetectResult ProcessChunks(ImageInfo* pImageInfo, DWORD& dwOffset);
	bool IsWriteProtected(void) { return m_pInfo->v1.writeProtected == 1; }
	BYTE GetOptimalBitTiming(void) { return (m_pInfo->v1.version >= 2) ? m_pInfo->optimalBitTiming : InfoChunkv2::optimalBitTiming5_25; }
	UINT GetMaxNibblesPerTrack(void) { return (m_pInfo->v1.version >= 2) ? m_pInfo->largestTrack*CWOZHelper::BLOCK_SIZE : WOZ1_TRACK_SIZE; }
	BYTE GetBootSectorFormat(void) { return (m_pInfo->v1.version >= 2) ? m_pInfo->bootSectorFormat : bootUnknown; }
	void InvalidateInfo(void) { m_pInfo = NULL; }
	BYTE* CreateEmptyDisk(DWORD& size);
#if _DEBUG
	BYTE* CreateEmptyDiskv1(DWORD& size);
#endif

	static const UINT32 ID1_WOZ1 = '1ZOW';	// 'WOZ1'
	static const UINT32 ID1_WOZ2 = '2ZOW';	// 'WOZ2'
	static const UINT32 ID2 = 0x0A0D0AFF;

	struct WOZHeader
	{
		UINT32	id1;		// 'WOZ1' or 'WOZ2'
		UINT32	id2;
		UINT32	crc32;
	};

	static const UINT32 MAX_TRACKS_5_25 = 40;
	static const UINT32 MAX_QUARTER_TRACKS_5_25 = MAX_TRACKS_5_25 * 4;
	static const UINT32 WOZ1_TRACK_SIZE = 6656;	// 0x1A00
	static const UINT32 WOZ1_TRK_OFFSET = 6646;
	static const UINT32 EMPTY_TRACK_SIZE = 6400;	// $C.5 blocks
	static const UINT32 BLOCK_SIZE = 512;
	static const BYTE TMAP_TRACK_EMPTY = 0xFF;
	static const UINT16 TRK_DEFAULT_BLOCK_COUNT_5_25 = 13;	// $D is default for TRKv2.blockCount

	static const BYTE bootUnknown = 0;
	static const BYTE bootSector16 = 1;
	static const BYTE bootSector13 = 2;
	static const BYTE bootSectorBoth = 3;

	struct WOZChunkHdr
	{
		UINT32 id;
		UINT32 size;
	};

	struct Tmap
	{
		BYTE tmap[MAX_QUARTER_TRACKS_5_25];
	};

	struct TRKv1
	{
		UINT16 bytesUsed;
		UINT16 bitCount;
		UINT16 splicePoint;
		BYTE spliceNibble;
		BYTE spliceBitCount;
		UINT16 reserved;
	};

	struct TRKv2
	{
		UINT16 startBlock;	// relative to start of file
		UINT16 blockCount;	// number of blocks for this BITS data
		UINT32 bitCount;
	};

	struct Trks
	{
		TRKv2 trks[MAX_QUARTER_TRACKS_5_25];
		BYTE bits[0];	// bits[] starts at offset 3 x BLOCK_SIZE = 1536
	};

private:
	static const UINT32 INFO_CHUNK_ID = 'OFNI';	// 'INFO'
	static const UINT32 TMAP_CHUNK_ID = 'PAMT';	// 'TMAP'
	static const UINT32 TRKS_CHUNK_ID = 'SKRT';	// 'TRKS'
	static const UINT32 WRIT_CHUNK_ID = 'TIRW';	// 'WRIT' - WOZv2
	static const UINT32 META_CHUNK_ID = 'ATEM';	// 'META'
	static const UINT32 INFO_CHUNK_SIZE = 60;	// Fixed size for both WOZv1 & WOZv2

	struct InfoChunk
	{
		BYTE	version;
		BYTE	diskType;
		BYTE	writeProtected;	// 1 = Floppy is write protected
		BYTE	synchronized;	// 1 = Cross track sync was used during imaging
		BYTE	cleaned;		// 1 = MC3470 fake bits have been removed
		BYTE	creator[32];	// Name of software that created the WOZ file.
								// String in UTF-8. No BOM. Padded to 32 bytes
								// using space character (0x20).

		static const BYTE diskType5_25 = 1;
		static const BYTE diskType3_5 = 2;
	};

	struct InfoChunkv2
	{
		InfoChunk v1;
		BYTE diskSides;			// 5.25 will always be 1; 3.5 can be 1 or 2
		BYTE bootSectorFormat;
		BYTE optimalBitTiming;	// in 125ns increments (And a standard bit rate for 5.25 disk would be 32 (4us))
		UINT16 compatibleHardware;
		UINT16 requiredRAM;		// in K (1024 bytes)
		UINT16 largestTrack;	// in blocks (512 bytes)

		static const BYTE optimalBitTiming5_25 = 32;
	};

	InfoChunkv2* m_pInfo;	// NB. image-specific - only valid during Detect(), which calls InvalidateInfo() when done

	//

	struct WOZEmptyImage525	// 5.25"
	{
		WOZHeader hdr;

		WOZChunkHdr infoHdr;
		InfoChunkv2 info;
		BYTE infoPadding[INFO_CHUNK_SIZE-sizeof(InfoChunkv2)];

		WOZChunkHdr tmapHdr;
		Tmap tmap;

		WOZChunkHdr trksHdr;
		Trks trks;
	};

	struct WOZv1EmptyImage525	// 5.25"
	{
		WOZHeader hdr;

		WOZChunkHdr infoHdr;
		InfoChunk info;
		BYTE infoPadding[INFO_CHUNK_SIZE-sizeof(InfoChunk)];

		WOZChunkHdr tmapHdr;
		Tmap tmap;

		WOZChunkHdr trksHdr;
	};
};

#pragma warning(pop)
#pragma pack(pop)

//-------------------------------------

class CImageHelperBase
{
public:
	CImageHelperBase(const bool bIsFloppy) :
		m_2IMGHelper(bIsFloppy),
		m_Result2IMG(eMismatch),
		m_WOZHelper()
	{
	}
	virtual ~CImageHelperBase(void)
	{
		for (UINT i=0; i<m_vecImageTypes.size(); i++)
			delete m_vecImageTypes[i];
	}

	ImageError_e Open(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, const bool bCreateIfNecessary, std::string& strFilenameInZip);
	void Close(ImageInfo* pImageInfo);
	bool WOZUpdateInfo(ImageInfo* pImageInfo, DWORD& dwOffset);

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, ImageInfo* pImageInfo) = 0;
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize) = 0;
	virtual UINT GetMaxImageSize(void) = 0;
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer) = 0;

protected:
	ImageError_e CheckGZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo);
	ImageError_e CheckZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, std::string& strFilenameInZip);
	ImageError_e CheckNormalFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, const bool bCreateIfNecessary);
	void GetCharLowerExt(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize);
	void GetCharLowerExt2(TCHAR* pszExt, LPCTSTR pszImageFilename, const UINT uExtSize);
	void SetImageInfo(ImageInfo* pImageInfo, FileType_e fileType, DWORD dwOffset, CImageBase* pImageType, DWORD dwSize);

	UINT GetNumImages(void) { return m_vecImageTypes.size(); };
	CImageBase* GetImage(UINT uIndex) { _ASSERT(uIndex<GetNumImages()); return m_vecImageTypes[uIndex]; }
	CImageBase* GetImage(eImageType Type)
	{
		if (Type == eImageUNKNOWN)
			return NULL;
		for (UINT i=0; i<GetNumImages(); i++)
		{
			if (m_vecImageTypes[i]->GetType() == Type)
				return m_vecImageTypes[i];
		}
		_ASSERT(0);
		return NULL;
	}

protected:
	typedef std::vector<CImageBase*> VECIMAGETYPE;
	VECIMAGETYPE m_vecImageTypes;

	C2IMGHelper m_2IMGHelper;
	eDetectResult m_Result2IMG;
	CWOZHelper m_WOZHelper;
};

//-------------------------------------

class CDiskImageHelper : public CImageHelperBase
{
public:
	CDiskImageHelper(void);
	virtual ~CDiskImageHelper(void) {}

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, ImageInfo* pImageInfo);
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize);
	virtual UINT GetMaxImageSize(void);
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer);

	UINT GetNumTracksInImage(CImageBase* pImageType) { return pImageType->m_uNumTracksInImage; }
	void SetNumTracksInImage(CImageBase* pImageType, UINT uNumTracks) { pImageType->m_uNumTracksInImage = uNumTracks; }

private:
	void SkipMacBinaryHdr(LPBYTE& pImage, DWORD& dwSize, DWORD& dwOffset);

private:
	CMacBinaryHelper m_MacBinaryHelper;
};

//-------------------------------------

class CHardDiskImageHelper : public CImageHelperBase
{
public:
	CHardDiskImageHelper(void);
	virtual ~CHardDiskImageHelper(void) {}

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, ImageInfo* pImageInfo);
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize);
	virtual UINT GetMaxImageSize(void);
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer);
};
