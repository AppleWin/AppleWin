#pragma once

// Structs used by save-state file

// *** DON'T CHANGE ANY STRUCT WITHOUT CONSIDERING BACKWARDS COMPATIBILITY WITH .AWS FORMAT ***

/////////////////////////////////////////////////////////////////////////////////

#define MAKE_VERSION(a,b,c,d) ((a<<24) | (b<<16) | (c<<8) | (d))

#define AW_SS_TAG 'SSWA'	// 'AWSS' = AppleWin SnapShot

struct SS_FILE_HDR
{
	DWORD dwTag;		// "AWSS"
	DWORD dwVersion;
	DWORD dwChecksum;
};

struct SS_UNIT_HDR
{
	union
	{
		struct
		{
			DWORD dwLength;		// Byte length of this unit struct
			DWORD dwVersion;
		} v1;
		struct
		{
			DWORD Length;		// Byte length of this unit struct
			WORD Type;			// SS_UNIT_TYPE
			WORD Version;		// Incrementing value from 1
		} v2;
	} hdr;
};

enum SS_UNIT_TYPE
{
	UT_Reserved = 0,
	UT_Apple2,
	UT_Card,
	UT_Config,
};

const UINT nMemMainSize = 64*1024;
const UINT nMemAuxSize = 64*1024;

struct SS_CARD_HDR
{
	SS_UNIT_HDR UnitHdr;
	DWORD Type;			// SS_CARDTYPE
	DWORD Slot;			// [1..7], 0=Language card, 8=Aux
};

enum SS_CARDTYPE
{
	CT_Empty = 0,
	CT_Disk2,			// Apple Disk][
	CT_SSC,				// Apple Super Serial Card
	CT_MockingboardC,	// Soundcard
	CT_GenericPrinter,
	CT_GenericHDD,		// Hard disk
	CT_GenericClock,
	CT_MouseInterface,
	CT_Z80,
	CT_Phasor,			// Soundcard
	CT_Echo,			// Soundcard
	CT_SAM,				// Soundcard: Software Automated Mouth
};

/////////////////////////////////////////////////////////////////////////////////

struct SS_CARD_EMPTY
{
	SS_CARD_HDR	Hdr;
};
