#pragma once

#define GZ_SUFFIX ".gz"
#define GZ_SUFFIX_LEN (sizeof(GZ_SUFFIX)-1)

#define ZIP_SUFFIX ".zip"
#define ZIP_SUFFIX_LEN (sizeof(ZIP_SUFFIX)-1)


enum eImageType {eImageUNKNOWN, eImageDO, eImagePO, eImageNIB1, eImageNIB2, eImageHDV, eImageIIE, eImageAPL, eImagePRG};
enum eDetectResult {eMismatch, ePossibleMatch, eMatch};

class CImageBase;
enum FileType_e {eFileNormal, eFileGZip, eFileZip};

struct ImageInfo
{
	TCHAR			szFilename[MAX_PATH];
	CImageBase*		pImageType;
	FileType_e		FileType;
	HANDLE			hFile;
	DWORD			uOffset;
	bool			bWriteProtected;
	UINT			uImageSize;
	char			szFilenameInZip[MAX_PATH];
	zip_fileinfo	zipFileInfo;
	UINT			uNumEntriesInZip;
	// Floppy only
	BYTE			ValidTrack[TRACKS_MAX];
	UINT			uNumTracks;
	BYTE*			pImageBuffer;
};

//-------------------------------------

#define HD_BLOCK_SIZE 512

#define UNIDISK35_800K_SIZE (800*1024)	// UniDisk 3.5"
#define HARDDISK_32M_SIZE (HD_BLOCK_SIZE * 65536)

#define DEFAULT_VOLUME_NUMBER 254

class CImageBase
{
public:
	CImageBase(void) : m_uNumTracksInImage(0), m_uVolumeNumber(DEFAULT_VOLUME_NUMBER) {}
	virtual ~CImageBase(void) {}

	virtual bool Boot(ImageInfo* pImageInfo) { return false; }
	virtual eDetectResult Detect(const LPBYTE pImage, const DWORD dwImageSize, const TCHAR* pszExt) = 0;
	virtual void Read(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImageBuffer, int* pNibbles) { }
	virtual bool Read(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer) { return false; }
	virtual void Write(ImageInfo* pImageInfo, int nTrack, int nQuarterTrack, LPBYTE pTrackImage, int nNibbles) { }
	virtual bool Write(ImageInfo* pImageInfo, UINT nBlock, LPBYTE pBlockBuffer) { return false; }

	virtual bool AllowBoot(void) { return false; }		// Only:    APL and PRG
	virtual bool AllowRW(void) { return true; }			// All but: APL and PRG
	virtual bool AllowCreate(void) { return false; }	// WE CREATE ONLY DOS ORDER (DO) OR 6656-NIBBLE (NIB) FORMAT FILES
	virtual UINT GetImageSizeForCreate(void) { _ASSERT(0); return (UINT)-1; }

	virtual eImageType GetType(void) = 0;
	virtual char* GetCreateExtensions(void) = 0;
	virtual char* GetRejectExtensions(void) = 0;

	void SetVolumeNumber(const BYTE uVolumeNumber) { m_uVolumeNumber = uVolumeNumber; }

	enum SectorOrder_e {eProDOSOrder, eDOSOrder, eSIMSYSTEMOrder, NUM_SECTOR_ORDERS};

protected:
	bool ReadTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);
	bool WriteTrack(ImageInfo* pImageInfo, const int nTrack, LPBYTE pTrackBuffer, const UINT uTrackSize);
	bool ReadBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer);
	bool WriteBlock(ImageInfo* pImageInfo, const int nBlock, LPBYTE pBlockBuffer);

	LPBYTE Code62(int sector);
	void Decode62(LPBYTE imageptr);
	void DenibblizeTrack (LPBYTE trackimage, SectorOrder_e SectorOrder, int nibbles);
	DWORD NibblizeTrack (LPBYTE trackimagebuffer, SectorOrder_e SectorOrder, int track);
	void SkewTrack (const int nTrack, const int nNumNibbles, const LPBYTE pTrackImageBuffer);
	bool IsValidImageSize(const DWORD uImageSize);

public:
	static LPBYTE ms_pWorkBuffer;
	UINT m_uNumTracksInImage;	// Init'd by CDiskImageHelper.Detect()/GetImageForCreation() & possibly updated by IsValidImageSize()

protected:
	static BYTE ms_DiskByte[0x40];
	static BYTE ms_SectorNumber[NUM_SECTOR_ORDERS][0x10];
	BYTE m_uVolumeNumber;
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
#pragma pack(1)	// Ensure Header2IMG is packed

class C2IMGHelper : public CHdrHelper
{
public:
	C2IMGHelper(const bool bIsFloppy) : m_bIsFloppy(bIsFloppy) {}
	virtual ~C2IMGHelper(void) {}
	virtual eDetectResult DetectHdr(LPBYTE& pImage, DWORD& dwImageSize, DWORD& dwOffset);
	virtual UINT GetMaxHdrSize(void) { return sizeof(Header2IMG); }
	BYTE GetVolumeNumber(void);
	bool IsLocked(void);

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

#pragma pack(pop)

//-------------------------------------

class CImageHelperBase
{
public:
	CImageHelperBase(const bool bIsFloppy) :
		m_2IMGHelper(bIsFloppy),
		m_Result2IMG(eMismatch)
	{
	}
	virtual ~CImageHelperBase(void)
	{
		for (UINT i=0; i<m_vecImageTypes.size(); i++)
			delete m_vecImageTypes[i];
	}

	ImageError_e Open(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, const bool bCreateIfNecessary, std::string& strFilenameInZip);
	void Close(ImageInfo* pImageInfo, const bool bDeleteFile);

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_) = 0;
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize) = 0;
	virtual UINT GetMaxImageSize(void) = 0;
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer) = 0;

protected:
	ImageError_e CheckGZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo);
	ImageError_e CheckZipFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, std::string& strFilenameInZip);
	ImageError_e CheckNormalFile(LPCTSTR pszImageFilename, ImageInfo* pImageInfo, const bool bCreateIfNecessary);

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
};

//-------------------------------------

class CDiskImageHelper : public CImageHelperBase
{
public:
	CDiskImageHelper(void);
	virtual ~CDiskImageHelper(void) {}

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_);
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize);
	virtual UINT GetMaxImageSize(void);
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer);

	UINT GetNumTracksInImage(CImageBase* pImageType) { return pImageType->m_uNumTracksInImage; }
	void SetNumTracksInImage(CImageBase* pImageType, UINT uNumTracks) { pImageType->m_uNumTracksInImage = uNumTracks; }

	LPBYTE GetWorkBuffer(void) { return CImageBase::ms_pWorkBuffer; }
	void SetWorkBuffer(LPBYTE pBuffer) { CImageBase::ms_pWorkBuffer = pBuffer; }

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

	virtual CImageBase* Detect(LPBYTE pImage, DWORD dwSize, const TCHAR* pszExt, DWORD& dwOffset, bool* pWriteProtected_);
	virtual CImageBase* GetImageForCreation(const TCHAR* pszExt, DWORD* pCreateImageSize);
	virtual UINT GetMaxImageSize(void);
	virtual UINT GetMinDetectSize(const UINT uImageSize, bool* pTempDetectBuffer);
};
