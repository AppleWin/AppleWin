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

/////////////////////////////////////////////////////////////////////////////////

struct SS_CARD_EMPTY
{
	SS_CARD_HDR	Hdr;
};

/////////////////////////////////////////////////////////////////////////////////

struct SY6522A
{
	BYTE ORB;				// $00 - Port B
	BYTE ORA;				// $01 - Port A (with handshaking)
	BYTE DDRB;				// $02 - Data Direction Register B
	BYTE DDRA;				// $03 - Data Direction Register A
	//
	// $04 - Read counter (L) / Write latch (L)
	// $05 - Read / Write & initiate count (H)
	// $06 - Read / Write & latch (L)
	// $07 - Read / Write & latch (H)
	// $08 - Read counter (L) / Write latch (L)
	// $09 - Read counter (H) / Write latch (H)
	USHORT TIMER1_COUNTER;
	USHORT TIMER1_LATCH;
	USHORT TIMER2_COUNTER;
	USHORT TIMER2_LATCH;
	int timer1IrqDelay;
	int timer2IrqDelay;
	//
	BYTE SERIAL_SHIFT;		// $0A
	BYTE ACR;				// $0B - Auxiliary Control Register
	BYTE PCR;				// $0C - Peripheral Control Register
	BYTE IFR;				// $0D - Interrupt Flag Register
	BYTE IER;				// $0E - Interrupt Enable Register
	BYTE ORA_NO_HS;			// $0F - Port A (without handshaking)
};

struct SSI263A
{
	BYTE DurationPhoneme;
	BYTE Inflection;		// I10..I3
	BYTE RateInflection;
	BYTE CtrlArtAmp;
	BYTE FilterFreq;
	//
	BYTE CurrentMode;		// b7:6=Mode; b0=D7 pin (for IRQ)
};
