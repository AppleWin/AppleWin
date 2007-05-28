/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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

/* Description: Super Serial Card emulation
 *
 * Author: Various
 */

// TO DO:
// . Enable & test Tx IRQ
// . DIP switch read values
//

// Refs:
// (1) "Super Serial Card (SSC) Memory Locations for Programmers" - Aaron Heiss
// (2) SSC recv IRQ example: http://www.wright.edu/~john.matthews/ssc.html#lst - John B. Matthews, 5/13/87
// (3) WaitCommEvent, etc: http://mail.python.org/pipermail/python-list/2002-November/131437.html
// (4) SY6551 info: http://www.axess.com/twilight/sock/rs232pak.html
//

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"

//#define SUPPORT_MODEM

// Default: 19200-8-N-1
// Maybe a better default is: 9600-7-N-1 (for HyperTrm)
SSC_DIPSW CSuperSerialCard::m_DIPSWDefault =
{
	// DIPSW1:
	CBR_19200,
	FWMODE_CIC,

	// DIPSW2:
	ONESTOPBIT,
	8,				// ByteSize
	NOPARITY,
	true,			// LF
	false,			// INT
};

//===========================================================================

CSuperSerialCard::CSuperSerialCard()
{
	m_dwSerialPort = 0;

	GetDIPSW();

	m_vRecvBytes = 0;

	m_hCommHandle = INVALID_HANDLE_VALUE;
	m_dwCommInactivity	= 0;

	m_bTxIrqEnabled = false;
	m_bRxIrqEnabled = false;

	m_bWrittenTx = false;

	m_vbCommIRQ = false;
	m_hCommThread = NULL;

	for (UINT i=0; i<COMMEVT_MAX; i++)
		m_hCommEvent[i] = NULL;

	memset(&m_o, 0, sizeof(m_o));
}

CSuperSerialCard::~CSuperSerialCard()
{
}

//===========================================================================

// TODO: Serial Comms - UI Property Sheet Page:
// . Ability to config the 2x DIPSWs - only takes affect after next Apple2 reset
// . 'Default' button that resets DIPSWs to DIPSWDefaults

void CSuperSerialCard::GetDIPSW()
{
	// TODO: Read settings from Registry
	// In the meantime, use the defaults:
	SetDIPSWDefaults();

	//

	m_uBaudRate	= m_DIPSWCurrent.uBaudRate;

	m_uStopBits	= m_DIPSWCurrent.uStopBits;
	m_uByteSize	= m_DIPSWCurrent.uByteSize;
	m_uParity	= m_DIPSWCurrent.uParity;

	//

	m_uControlByte	= GenerateControl();
	m_uCommandByte	= 0x00;
}

void CSuperSerialCard::SetDIPSWDefaults()
{
	// Default DIPSW settings (comms mode)

	// DIPSW1:
	m_DIPSWCurrent.uBaudRate		= m_DIPSWDefault.uBaudRate;
	m_DIPSWCurrent.eFirmwareMode	= m_DIPSWDefault.eFirmwareMode;

	// DIPSW2:
	m_DIPSWCurrent.uStopBits		= m_DIPSWDefault.uStopBits;
	m_DIPSWCurrent.uByteSize		= m_DIPSWDefault.uByteSize;
	m_DIPSWCurrent.uParity			= m_DIPSWDefault.uParity;
	m_DIPSWCurrent.bLinefeed		= m_DIPSWDefault.bLinefeed;
	m_DIPSWCurrent.bInterrupts		= m_DIPSWDefault.bInterrupts;
}

BYTE CSuperSerialCard::GenerateControl()
{
	const UINT CLK=1;	// Internal

	UINT bmByteSize = (8 - m_uByteSize);	// [8,7,6,5] -> [0,1,2,3]
	_ASSERT(bmByteSize <= 3);

	UINT StopBit;
	if (	((m_uByteSize == 8) && (m_uParity != NOPARITY))	||
			( m_uStopBits != ONESTOPBIT	)	)
		StopBit = 1;
	else
		StopBit = 0;

	return (StopBit<<7) | (bmByteSize<<5) | (CLK<<4) | BaudRateToIndex(m_uBaudRate);
}

UINT CSuperSerialCard::BaudRateToIndex(UINT uBaudRate)
{
	switch (uBaudRate)
	{
	case CBR_110: return 0x05;
	case CBR_300: return 0x06;
	case CBR_600: return 0x07;
	case CBR_1200: return 0x08;
	case CBR_2400: return 0x0A;
	case CBR_4800: return 0x0C;
	case CBR_9600: return 0x0E;
	case CBR_19200: return 0x0F;
	}

	_ASSERT(0);
	return BaudRateToIndex(CBR_9600);
}

//===========================================================================

void CSuperSerialCard::UpdateCommState()
{
	if (m_hCommHandle == INVALID_HANDLE_VALUE)
		return;

	DCB dcb;
	ZeroMemory(&dcb,sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	GetCommState(m_hCommHandle,&dcb);
	dcb.BaudRate = m_uBaudRate;
	dcb.ByteSize = m_uByteSize;
	dcb.Parity   = m_uParity;
	dcb.StopBits = m_uStopBits;

	SetCommState(m_hCommHandle,&dcb);
}

//===========================================================================

BOOL CSuperSerialCard::CheckComm()
{
	m_dwCommInactivity = 0;

	if ((m_hCommHandle == INVALID_HANDLE_VALUE) && m_dwSerialPort)
	{
		TCHAR portname[8];
		wsprintf(portname, TEXT("COM%u"), m_dwSerialPort);

		m_hCommHandle = CreateFile(portname,
								GENERIC_READ | GENERIC_WRITE,
								0,								// exclusive access
								(LPSECURITY_ATTRIBUTES)NULL,	// default security attributes
								OPEN_EXISTING,
								FILE_FLAG_OVERLAPPED,			// required for WaitCommEvent()
								NULL);

		if (m_hCommHandle != INVALID_HANDLE_VALUE)
		{
			UpdateCommState();
			COMMTIMEOUTS ct;
			ZeroMemory(&ct,sizeof(COMMTIMEOUTS));
			ct.ReadIntervalTimeout = MAXDWORD;
			SetCommTimeouts(m_hCommHandle,&ct);
			CommThInit();
		}
		else
		{
			DWORD uError = GetLastError();
		}
	}

	return (m_hCommHandle != INVALID_HANDLE_VALUE);
}

//===========================================================================

void CSuperSerialCard::CloseComm()
{
	CommThUninit();		// Kill CommThread before closing COM handle

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
		CloseHandle(m_hCommHandle);

	m_hCommHandle = INVALID_HANDLE_VALUE;
	m_dwCommInactivity = 0;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::SSC_IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CSuperSerialCard* pSSC = (CSuperSerialCard*) MemGetSlotParameters(uSlot);

	switch (uAddr & 0xf)
	{
	case 0x0:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x1:	return pSSC->CommDipSw(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x2:	return pSSC->CommDipSw(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x3:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x4:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x5:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x6:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x7:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x8:	return pSSC->CommReceive(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x9:	return pSSC->CommStatus(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xA:	return pSSC->CommCommand(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xB:	return pSSC->CommControl(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xC:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xD:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xE:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xF:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	}

	return 0;
}

BYTE __stdcall CSuperSerialCard::SSC_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CSuperSerialCard* pSSC = (CSuperSerialCard*) MemGetSlotParameters(uSlot);

	switch (uAddr & 0xf)
	{
	case 0x0:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x1:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x2:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x3:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x4:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x5:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x6:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x7:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x8:	return pSSC->CommTransmit(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0x9:	return pSSC->CommStatus(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xA:	return pSSC->CommCommand(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xB:	return pSSC->CommControl(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xC:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xD:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xE:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	case 0xF:	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
	}

	return 0;
}

//===========================================================================

// EG. 0x09 = Enable IRQ, No parity [Ref.2]

BYTE __stdcall CSuperSerialCard::CommCommand(WORD, WORD, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value	!= m_uCommandByte))
	{
		m_uCommandByte	= value;

		// UPDATE THE PARITY
		if (m_uCommandByte	& 0x20)
		{
			switch (m_uCommandByte	& 0xC0)
			{
			case 0x00 :	m_uParity = ODDPARITY; break;
			case 0x40 :	m_uParity = EVENPARITY; break;
			case 0x80 :	m_uParity = MARKPARITY; break;
			case 0xC0 :	m_uParity = SPACEPARITY; break;
			}
		}
		else
		{
			m_uParity = NOPARITY;
		}

		if (m_uCommandByte	& 0x10)	// Receiver mode echo [0=no echo, 1=echo]
		{
		}

		switch (m_uCommandByte	& 0x0C)	// transmitter interrupt control
		{
			// Note: the RTS signal must be set 'low' in order to receive any
			// incoming data from the serial device
			case 0<<2: // set RTS high and transmit no interrupts
				m_bTxIrqEnabled = false;
				break;
			case 1<<2: // set RTS low and transmit interrupts
				m_bTxIrqEnabled = true;
				break;
			case 2<<2: // set RTS low and transmit no interrupts
				m_bTxIrqEnabled = false;
				break;
			case 3<<2: // set RTS low and transmit break signals instead of interrupts
				m_bTxIrqEnabled = false;
				break;
		}

		// interrupt request disable [0=enable receiver interrupts]
		m_bRxIrqEnabled = ((m_uCommandByte & 0x02) == 0);

		if (m_uCommandByte	& 0x01)	// Data Terminal Ready (DTR) setting [0=set DTR high (indicates 'not ready')]
		{
			// Note that, although the DTR is generally not used in the SSC (it may actually not
			// be connected!), it must be set to 'low' in order for the 6551 to function correctly.
		}

		UpdateCommState();
	}

	return m_uCommandByte;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::CommControl(WORD, WORD, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value != m_uControlByte))
	{
		m_uControlByte = value;

		// UPDATE THE BAUD RATE
		switch (m_uControlByte & 0x0F)
		{
			// Note that 1 MHz Apples (everything other than the Apple IIgs and //c
			// Plus running in "fast" mode) cannot handle 19.2 kbps, and even 9600
			// bps on these machines requires either some highly optimised code or
			// a decent buffer in the device being accessed.  The faster Apples
			// have no difficulty with this speed, however.

			case 0x00: // fall through [16x external clock]
			case 0x01: // fall through [50 bps]
			case 0x02: // fall through [75 bps]
			case 0x03: // fall through [109.92 bps]
			case 0x04: // fall through [134.58 bps]
			case 0x05: m_uBaudRate = CBR_110;     break;	// [150 bps]
			case 0x06: m_uBaudRate = CBR_300;     break;
			case 0x07: m_uBaudRate = CBR_600;     break;
			case 0x08: m_uBaudRate = CBR_1200;    break;
			case 0x09: // fall through [1800 bps]
			case 0x0A: m_uBaudRate = CBR_2400;    break;
			case 0x0B: // fall through [3600 bps]
			case 0x0C: m_uBaudRate = CBR_4800;    break;
			case 0x0D: // fall through [7200 bps]
			case 0x0E: m_uBaudRate = CBR_9600;    break;
			case 0x0F: m_uBaudRate = CBR_19200;   break;
		}

		if (m_uControlByte & 0x10)
		{
			// receiver clock source [0= external, 1= internal]
		}

		// UPDATE THE BYTE SIZE
		switch (m_uControlByte & 0x60)
		{
			case 0x00: m_uByteSize = 8;  break;
			case 0x20: m_uByteSize = 7;  break;
			case 0x40: m_uByteSize = 6;  break;
			case 0x60: m_uByteSize = 5;  break;
		}

		// UPDATE THE NUMBER OF STOP BITS
		if (m_uControlByte & 0x80)
		{
			if ((m_uByteSize == 8) && (m_uParity != NOPARITY))
				m_uStopBits = ONESTOPBIT;
			else if ((m_uByteSize == 5) && (m_uParity == NOPARITY))
				m_uStopBits = ONE5STOPBITS;
			else
				m_uStopBits = TWOSTOPBITS;
		}
		else
		{
			m_uStopBits = ONESTOPBIT;
		}

		UpdateCommState();
	}

  return m_uControlByte;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::CommReceive(WORD, WORD, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return 0;

	BYTE result = 0;
	if (m_vRecvBytes)
	{
		// Don't need critical section in here as CommThread is waiting for ACK

		result = m_RecvBuffer[0];
		--m_vRecvBytes;

		if (m_vbCommIRQ && !m_vRecvBytes)
		{
			// Read last byte, so get CommThread to call WaitCommEvent() again
			OutputDebugString("CommRecv: SetEvent - ACK\n");
			SetEvent(m_hCommEvent[COMMEVT_ACK]);
		}
	}

	return result;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::CommTransmit(WORD, WORD, BYTE, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	DWORD uBytesWritten;
	WriteFile(m_hCommHandle, &value, 1, &uBytesWritten, &m_o);

	m_bWrittenTx = true;	// Transmit done

	// TO DO:
	// 1) Use CommThread determine when transmit is complete
	// 2) OR do this:
	//if (m_bTxIrqEnabled)
	//	CpuIrqAssert(IS_SSC);

	return 0;
}

//===========================================================================

// 6551 ACIA Status Register ($C089+s0)
// ------------------------------------
// Bit 	Value 	Meaning
// 0    1       Parity error
// 1    1       Framing error
// 2    1       Overrun error
// 3    1       Receive register full
// 4    1       Transmit register empty
// 5    0       Data Carrier Detect (DCD) true [0=DCD low (detected), 1=DCD high (not detected)]
// 6    0       Data Set Ready (DSR) true [0=DSR low (ready), 1=DSR high (not ready)]
// 7    1       Interrupt (IRQ) true (cleared by reading status reg [Ref.4])

enum {	ST_PARITY_ERR	= 1<<0,
		ST_FRAMING_ERR	= 1<<1,
		ST_OVERRUN_ERR	= 1<<2,
		ST_RX_FULL		= 1<<3,
		ST_TX_EMPTY		= 1<<4,
		ST_DCD			= 1<<5,
		ST_DSR			= 1<<6,
		ST_IRQ			= 1<<7
	};

BYTE __stdcall CSuperSerialCard::CommStatus(WORD, WORD, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return ST_DSR | ST_DCD | ST_TX_EMPTY;

#ifdef SUPPORT_MODEM
	DWORD modemstatus = 0;
	GetCommModemStatus(m_hCommHandle,&modemstatus);				// Returns 0x30 = MS_DSR_ON|MS_CTS_ON
#endif

	//

	// TO DO - ST_TX_EMPTY:
	// . IRQs enabled  : set after WaitCommEvent has signaled that TX has completed
	// . IRQs disabled : always set it [Currently done]
	//

	// So that /m_vRecvBytes/ doesn't change midway (from 0 to 1):
	// . bIRQ=false, but uStatus.ST_RX_FULL=1
	EnterCriticalSection(&m_CriticalSection);

	bool bIRQ = false;
	if (m_bTxIrqEnabled && m_bWrittenTx)
	{
		bIRQ = true;
	}
	if (m_bRxIrqEnabled && m_vRecvBytes)
	{
		bIRQ = true;
	}

	m_bWrittenTx = false;	// Read status reg always clears IRQ

	//

	BYTE uStatus = ST_TX_EMPTY 
				| (m_vRecvBytes					? ST_RX_FULL : 0x00)
#ifdef SUPPORT_MODEM
				| ((modemstatus & MS_RLSD_ON)	? 0x00 : ST_DCD)	// Need 0x00 to allow ZLink to start up
				| ((modemstatus & MS_DSR_ON)	? 0x00 : ST_DSR)
#endif
				| (bIRQ							? ST_IRQ : 0x00);

	LeaveCriticalSection(&m_CriticalSection);

	CpuIrqDeassert(IS_SSC);

	return uStatus;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::CommDipSw(WORD, WORD addr, BYTE, BYTE, ULONG)
{
	BYTE sw = 0;
	switch (addr & 0xf)
	{
	case 1:	// DIPSW1
		sw = (BaudRateToIndex(m_DIPSWCurrent.uBaudRate)<<4) | m_DIPSWCurrent.eFirmwareMode;
		break;

	case 2:	// DIPSW2
		// Comms mode - SSC manual, pg23/24
		BYTE INT = m_DIPSWCurrent.uStopBits == TWOSTOPBITS ? 1 : 0;	// SW2-1 (Stop bits: 1-ON(0); 2-OFF(1))
		BYTE DSR = 0;												// Always zero
		BYTE DCD = m_DIPSWCurrent.uByteSize == 7 ? 1 : 0;			// SW2-2 (Data bits: 8-ON(0); 7-OFF(1))
		BYTE TDR = 0;												// Always zero

		// SW2-3 (Parity: odd-ON(0); even-OFF(1))
		// SW2-4 (Parity: none-ON(0); SW2-3-OFF(1))
		BYTE RDR,OVR;
		switch (m_DIPSWCurrent.uParity)
		{
		case ODDPARITY:
			RDR = 0; OVR = 1;
			break;
		case EVENPARITY:
			RDR = 1; OVR = 1;
			break;
		default:
			_ASSERT(0);
		case NOPARITY:
			RDR = 0; OVR = 0;
			break;
		}

		BYTE FE = m_DIPSWCurrent.bLinefeed ? 1 : 0;					// SW2-5 (LF: yes-ON(0); no-OFF(1))
		BYTE PE = m_DIPSWCurrent.bInterrupts ? 1 : 0;				// SW2-6 (Interrupts: yes-ON(0); no-OFF(1))

		sw = (INT<<7) | (DSR<<6) | (DCD<<5) | (TDR<<4) | (RDR<<3) | (OVR<<2) | (FE<<1) | (PE<<0);
		break;
	}
	return sw;
}

//===========================================================================

void CSuperSerialCard::CommInitialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	const UINT SSC_FW_SIZE = 2*1024;
	const UINT SSC_SLOT_FW_SIZE = 256;
	const UINT SSC_SLOT_FW_OFFSET = 7*256;

	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_SSC_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != SSC_FW_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	memcpy(pCxRomPeripheral + uSlot*256, pData+SSC_SLOT_FW_OFFSET, SSC_SLOT_FW_SIZE);

	// Expansion ROM
	if (m_pExpansionRom == NULL)
	{
		m_pExpansionRom = new BYTE [SSC_FW_SIZE];

		if (m_pExpansionRom)
			memcpy(m_pExpansionRom, pData, SSC_FW_SIZE);
	}

	//

	RegisterIoHandler(uSlot, &CSuperSerialCard::SSC_IORead, &CSuperSerialCard::SSC_IOWrite, NULL, NULL, this, m_pExpansionRom);
}

//===========================================================================

void CSuperSerialCard::CommReset()
{
	CloseComm();

	GetDIPSW();

	m_vRecvBytes = 0;

	//

	m_bTxIrqEnabled = false;
	m_bRxIrqEnabled = false;

	m_bWrittenTx = false;

	m_vbCommIRQ = false;
}

//===========================================================================

void CSuperSerialCard::CommDestroy()
{
	CommReset();

	delete [] m_pExpansionRom;
	m_pExpansionRom = NULL;
}

//===========================================================================

void CSuperSerialCard::CommSetSerialPort(HWND window, DWORD newserialport)
{
	if (m_hCommHandle == INVALID_HANDLE_VALUE)
	{
		m_dwSerialPort = newserialport;
	}
	else
	{
		MessageBox(window,
			TEXT("You cannot change the serial port while it is ")
			TEXT("in use."),
			TEXT("Configuration"),
			MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}
}

//===========================================================================

void CSuperSerialCard::CommUpdate(DWORD totalcycles)
{
	if (m_hCommHandle == INVALID_HANDLE_VALUE)
		return;

	if ((m_dwCommInactivity += totalcycles) > 1000000)
	{
		static DWORD lastcheck = 0;

		if ((m_dwCommInactivity > 2000000) || (m_dwCommInactivity-lastcheck > 99950))
		{
#ifdef SUPPORT_MODEM
			DWORD modemstatus = 0;
			GetCommModemStatus(m_hCommHandle,&modemstatus);
			if ((modemstatus & MS_RLSD_ON) || DiskIsSpinning())
				m_dwCommInactivity = 0;
#else
			if (DiskIsSpinning())
				m_dwCommInactivity = 0;
#endif
		}

		//if (m_dwCommInactivity > 2000000)
		//	CloseComm();
	}
}

//===========================================================================

void CSuperSerialCard::CheckCommEvent(DWORD dwEvtMask)
{
	if (dwEvtMask & EV_RXCHAR)
	{
		EnterCriticalSection(&m_CriticalSection);
		ReadFile(m_hCommHandle, m_RecvBuffer, 1, (DWORD*)&m_vRecvBytes, &m_o);
		LeaveCriticalSection(&m_CriticalSection);

		if (m_bRxIrqEnabled && m_vRecvBytes)
		{
			m_vbCommIRQ = true;
			CpuIrqAssert(IS_SSC);
		}
	}
	//else if (dwEvtMask & EV_TXEMPTY)
	//{
	//	if (m_bTxIrqEnabled)
	//	{
	//		m_vbCommIRQ = true;
	//		CpuIrqAssert(IS_SSC);
	//	}
	//}
}

DWORD WINAPI CSuperSerialCard::CommThread(LPVOID lpParameter)
{
	CSuperSerialCard* pSSC = (CSuperSerialCard*) lpParameter;

	char szDbg[100];
//	BOOL bRes = SetCommMask(pSSC->m_hCommHandle, EV_TXEMPTY | EV_RXCHAR);
	BOOL bRes = SetCommMask(pSSC->m_hCommHandle, EV_RXCHAR);		// Just RX
	if (!bRes)
		return -1;

	//

	const UINT nNumEvents = 2;
#if 1
	HANDLE hCommEvent_Wait[nNumEvents] = {pSSC->m_hCommEvent[COMMEVT_WAIT], pSSC->m_hCommEvent[COMMEVT_TERM]};
	HANDLE hCommEvent_Ack[nNumEvents]  = {pSSC->m_hCommEvent[COMMEVT_ACK],  pSSC->m_hCommEvent[COMMEVT_TERM]};
#else
	HANDLE hCommEvent_Wait[nNumEvents];
	HANDLE hCommEvent_Ack[nNumEvents];

	hCommEvent_Wait[0] = m_hCommEvent[COMMEVT_WAIT];
	hCommEvent_Wait[1] = m_hCommEvent[COMMEVT_TERM];

	hCommEvent_Ack[0] = m_hCommEvent[COMMEVT_ACK];
	hCommEvent_Ack[1] = m_hCommEvent[COMMEVT_TERM];
#endif

	while(1)
	{
		DWORD dwEvtMask = 0;
		DWORD dwWaitResult;

		bRes = WaitCommEvent(pSSC->m_hCommHandle, &dwEvtMask, &pSSC->m_o);	// Will return immediately (probably with ERROR_IO_PENDING)
		_ASSERT(!bRes);
		if (!bRes)
		{
			DWORD dwRet = GetLastError();
			// Got this error once: ERROR_OPERATION_ABORTED
			_ASSERT(dwRet == ERROR_IO_PENDING);
			if (dwRet != ERROR_IO_PENDING)
				return -1;

			//
			// Wait for comm event
			//

			while(1)
			{
				OutputDebugString("CommThread: Wait1\n");
				dwWaitResult = WaitForMultipleObjects( 
										nNumEvents,			// number of handles in array
										hCommEvent_Wait,	// array of event handles
										FALSE,				// wait until any one is signaled
										INFINITE);

				// On very 1st wait *only*: get a false signal (when not running via debugger)
				if ((dwWaitResult == WAIT_OBJECT_0) && (dwEvtMask == 0))
					continue;

				if ((dwWaitResult >= WAIT_OBJECT_0) && (dwWaitResult <= WAIT_OBJECT_0+nNumEvents-1))
					break;
			}

			dwWaitResult -= WAIT_OBJECT_0;			// Determine event # that signaled
			sprintf(szDbg, "CommThread: GotEvent1: %d\n", dwWaitResult); OutputDebugString(szDbg);

			if (dwWaitResult == (nNumEvents-1))
				break;	// Termination event
		}

		// Comm event
		pSSC->CheckCommEvent(dwEvtMask);

		if (pSSC->m_vbCommIRQ)
		{
			//
			// Wait for ack
			//

			while(1)
			{
				OutputDebugString("CommThread: Wait2\n");
				dwWaitResult = WaitForMultipleObjects( 
										nNumEvents,			// number of handles in array
										hCommEvent_Ack,		// array of event handles
										FALSE,				// wait until any one is signaled
										INFINITE);

				if ((dwWaitResult >= WAIT_OBJECT_0) && (dwWaitResult <= WAIT_OBJECT_0+nNumEvents-1))
					break;
			}

			dwWaitResult -= WAIT_OBJECT_0;			// Determine event # that signaled
			sprintf(szDbg, "CommThread: GotEvent2: %d\n", dwWaitResult); OutputDebugString(szDbg);

			if (dwWaitResult == (nNumEvents-1))
				break;	// Termination event

			pSSC->m_vbCommIRQ = false;
		}
	}

	return 0;
}

bool CSuperSerialCard::CommThInit()
{
	_ASSERT(m_hCommThread == NULL);
	_ASSERT(m_hCommHandle);

	if ((m_hCommEvent[0] == NULL) && (m_hCommEvent[1] == NULL) && (m_hCommEvent[2] == NULL))
	{
		// Create an event object for use by WaitCommEvent

		m_o.hEvent = CreateEvent(
			NULL,   // default security attributes 
			FALSE,  // auto reset event (bManualReset)
			FALSE,  // not signaled (bInitialState)
			NULL    // no name
			);

		// Initialize the rest of the OVERLAPPED structure to zero
		m_o.Internal = 0;
		m_o.InternalHigh = 0;
		m_o.Offset = 0;
		m_o.OffsetHigh = 0;

		//

		m_hCommEvent[COMMEVT_WAIT] = m_o.hEvent;
		m_hCommEvent[COMMEVT_ACK] = CreateEvent(NULL,	// lpEventAttributes
										FALSE,	// bManualReset (FALSE = auto-reset)
										FALSE,	// bInitialState (FALSE = non-signaled)
										NULL);	// lpName
		m_hCommEvent[COMMEVT_TERM] = CreateEvent(NULL,	// lpEventAttributes
										FALSE,	// bManualReset (FALSE = auto-reset)
										FALSE,	// bInitialState (FALSE = non-signaled)
										NULL);	// lpName

		if ((m_hCommEvent[0] == NULL) || (m_hCommEvent[1] == NULL) || (m_hCommEvent[2] == NULL))
		{
			if(g_fh) fprintf(g_fh, "Comm: CreateEvent failed\n");
			return false;
		}
	}

	//

	if (m_hCommThread == NULL)
	{
		DWORD dwThreadId;

		m_hCommThread = CreateThread(NULL,			// lpThreadAttributes
									0,				// dwStackSize
									(LPTHREAD_START_ROUTINE) &CSuperSerialCard::CommThread,
									this,			// lpParameter
									0,				// dwCreationFlags : 0 = Run immediately
									&dwThreadId);	// lpThreadId

		InitializeCriticalSection(&m_CriticalSection);
	}

	return true;
}

void CSuperSerialCard::CommThUninit()
{
	if (m_hCommThread)
	{
		SetEvent(m_hCommEvent[COMMEVT_TERM]);	// Signal to thread that it should exit

		do
		{
			DWORD dwExitCode;
			if(GetExitCodeThread(m_hCommThread, &dwExitCode))
			{
				if(dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		}
		while(1);

		CloseHandle(m_hCommThread);
		m_hCommThread = NULL;

	  	DeleteCriticalSection(&m_CriticalSection);
	}

	//

	for (UINT i=0; i<COMMEVT_MAX; i++)
	{
		if(m_hCommEvent[i])
		{
			CloseHandle(m_hCommEvent[i]);
			m_hCommEvent[i] = NULL;
		}
	}
}

//===========================================================================

DWORD CSuperSerialCard::CommGetSnapshot(SS_IO_Comms* pSS)
{
	pSS->baudrate		= m_uBaudRate;
	pSS->bytesize		= m_uByteSize;
	pSS->commandbyte	= m_uCommandByte;
	pSS->comminactivity	= m_dwCommInactivity;
	pSS->controlbyte	= m_uControlByte;
	pSS->parity			= m_uParity;
	memcpy(pSS->recvbuffer, m_RecvBuffer, uRecvBufferSize);
	pSS->recvbytes		= m_vRecvBytes;
	pSS->stopbits		= m_uStopBits;
	return 0;
}

DWORD CSuperSerialCard::CommSetSnapshot(SS_IO_Comms* pSS)
{
	m_uBaudRate		= pSS->baudrate;
	m_uByteSize		= pSS->bytesize;
	m_uCommandByte		= pSS->commandbyte;
	m_dwCommInactivity	= pSS->comminactivity;
	m_uControlByte		= pSS->controlbyte;
	m_uParity			= pSS->parity;
	memcpy(m_RecvBuffer, pSS->recvbuffer, uRecvBufferSize);
	m_vRecvBytes	= pSS->recvbytes;
	m_uStopBits		= pSS->stopbits;
	return 0;
}
