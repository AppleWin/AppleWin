/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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
#define TCP_SERIAL_PORT 1977

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

CSuperSerialCard::CSuperSerialCard() :
	m_aySerialPortChoices(NULL),
	m_uTCPChoiceItemIdx(0)
{
	memset(m_ayCurrentSerialPortName, 0, sizeof(m_ayCurrentSerialPortName));
	m_dwSerialPortItem = 0;

	m_hCommHandle = INVALID_HANDLE_VALUE;
	m_hCommListenSocket = INVALID_SOCKET;
	m_hCommAcceptSocket = INVALID_SOCKET;
	m_dwCommInactivity	= 0;

	m_hCommThread = NULL;

	for (UINT i=0; i<COMMEVT_MAX; i++)
		m_hCommEvent[i] = NULL;

	memset(&m_o, 0, sizeof(m_o));

	InternalReset();
}

void CSuperSerialCard::InternalReset()
{
	GetDIPSW();

	m_bTxIrqEnabled = false;
	m_bRxIrqEnabled = false;

	m_bWrittenTx = false;

	m_vuRxCurrBuffer = 0;

	m_vbTxIrqPending = false;
	m_vbRxIrqPending = false;

	m_qComSerialBuffer[0].clear();
	m_qComSerialBuffer[1].clear();
	m_qTcpSerialBuffer.clear();
}

CSuperSerialCard::~CSuperSerialCard()
{
	delete [] m_aySerialPortChoices;
}

//===========================================================================

// TODO: Serial Comms - UI Property Sheet Page:
// . Ability to config the 2x DIPSWs - only takes affect after next Apple2 reset
// . 'Default' button that resets DIPSWs to DIPSWDefaults
// . Need to respect IRQ disable dipswitch (cannot be overridden by software)

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
	case CBR_115200: return 0x00;
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

	// check for COM or TCP socket handle, and setup if invalid
	if (IsActive())
		return true;

	if (m_dwSerialPortItem == m_uTCPChoiceItemIdx)
	{
		// init Winsock 1.1 (for Win95, otherwise could use 2.2)
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(1, 1), &wsaData) == 0) // or (2, 2) for Winsock 2.2
		{
			if (wsaData.wVersion != 0x0101) // or 0x0202 for Winsock 2.2
			{
				WSACleanup();
				return FALSE;
			}

			// initialized, so try to create a socket
			m_hCommListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (m_hCommListenSocket == INVALID_SOCKET)
			{
				WSACleanup();
				return FALSE;
			}

			// have socket so attempt to bind it
			SOCKADDR_IN saAddress;
			saAddress.sin_family = AF_INET;
			saAddress.sin_port = htons(TCP_SERIAL_PORT); // TODO: get from registry / GUI
			saAddress.sin_addr.s_addr = htonl(INADDR_ANY);
			if (bind(m_hCommListenSocket, (LPSOCKADDR)&saAddress, sizeof(saAddress)) == SOCKET_ERROR)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return FALSE;
			}

			// bound, so listen
			if (listen(m_hCommListenSocket, 1) == SOCKET_ERROR)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return FALSE;
			}

			// now send async events to our app's message handler
			if (WSAAsyncSelect(
					/* SOCKET s */ m_hCommListenSocket,
					/* HWND hWnd */ g_hFrameWindow,
					/* unsigned int wMsg */ WM_USER_TCP_SERIAL,
					/* long lEvent */ (FD_ACCEPT | FD_CONNECT | FD_READ | FD_CLOSE)) != 0)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return FALSE;
			}
		}
	}
	else if (m_dwSerialPortItem)
	{
		_ASSERT(m_dwSerialPortItem < m_vecSerialPortsItems.size()-1);	// size()-1 is TCP item
		TCHAR portname[SIZEOF_SERIALCHOICE_ITEM];
		wsprintf(portname, TEXT("COM%u"), m_vecSerialPortsItems[m_dwSerialPortItem]);

		m_hCommHandle = CreateFile(portname,
								GENERIC_READ | GENERIC_WRITE,
								0,								// exclusive access
								(LPSECURITY_ATTRIBUTES)NULL,	// default security attributes
								OPEN_EXISTING,
								FILE_FLAG_OVERLAPPED,			// required for WaitCommEvent()
								NULL);

		if (m_hCommHandle != INVALID_HANDLE_VALUE)
		{
			//BOOL bRes = SetupComm(m_hCommHandle, 8192, 8192);
			//_ASSERT(bRes);

			UpdateCommState();

			// ReadIntervalTimeout=MAXDWORD; ReadTotalTimeoutConstant=ReadTotalTimeoutMultiplier=0:
			// Read operation is to return immediately with the bytes that have already been received,
			// even if no bytes have been received.
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

	return IsActive();
}

//===========================================================================

void CSuperSerialCard::CloseComm()
{
	CommTcpSerialCleanup();	// Shut down Winsock

	CommThUninit();		// Kill CommThread before closing COM handle

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
		CloseHandle(m_hCommHandle);

	m_hCommHandle = INVALID_HANDLE_VALUE;
	m_dwCommInactivity = 0;
}

//===========================================================================

void CSuperSerialCard::CommTcpSerialCleanup()
{
	if (m_hCommListenSocket != INVALID_SOCKET)
	{
		WSAAsyncSelect(m_hCommListenSocket, g_hFrameWindow, 0, 0); // Stop event messages
		closesocket(m_hCommListenSocket);
		m_hCommListenSocket = INVALID_SOCKET;

		CommTcpSerialClose();

		WSACleanup();
	}
}

//===========================================================================

void CSuperSerialCard::CommTcpSerialClose()
{
	if (m_hCommAcceptSocket != INVALID_SOCKET)
	{
		shutdown(m_hCommAcceptSocket, 2 /* SD_BOTH */); // In case the client is waiting for data
		closesocket(m_hCommAcceptSocket);
		m_hCommAcceptSocket = INVALID_SOCKET;
	}

	m_qTcpSerialBuffer.clear();
}

//===========================================================================

void CSuperSerialCard::CommTcpSerialAccept()
{
	// Valid listener socket and invalid accept socket?
	if ((m_hCommListenSocket != INVALID_SOCKET) && (m_hCommAcceptSocket == INVALID_SOCKET))
	{
		// Y: accept the connection
		m_hCommAcceptSocket = accept(m_hCommListenSocket, NULL, NULL );
	}
}

//===========================================================================

void CSuperSerialCard::CommTcpSerialReceive()
{
	if (m_hCommAcceptSocket != INVALID_SOCKET)
	{
		char Data[0x80];
		int nReceived = 0;
		while ((nReceived = recv(m_hCommAcceptSocket, Data, sizeof(Data), 0)) > 0)
		{
			for (int i = 0; i < nReceived; i++)
			{
				m_qTcpSerialBuffer.push_back(Data[i]);
			}
		}

		if (m_bRxIrqEnabled && !m_qTcpSerialBuffer.empty())
		{
			CpuIrqAssert(IS_SSC);
		}
	}
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

		// interrupt request disable [0=enable receiver interrupts] - NOTE: SSC docs get this wrong!
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

			case 0x00: m_uBaudRate = CBR_115200;	break;	// Internal clk: undoc'd 115.2K (or 16x external clock)
			case 0x01: // fall through [50 bps]
			case 0x02: // fall through [75 bps]
			case 0x03: // fall through [109.92 bps]
			case 0x04: // fall through [134.58 bps]
			case 0x05: m_uBaudRate = CBR_110;		break;	// [150 bps]
			case 0x06: m_uBaudRate = CBR_300;		break;
			case 0x07: m_uBaudRate = CBR_600;		break;
			case 0x08: m_uBaudRate = CBR_1200;		break;
			case 0x09: // fall through [1800 bps]
			case 0x0A: m_uBaudRate = CBR_2400;		break;
			case 0x0B: // fall through [3600 bps]
			case 0x0C: m_uBaudRate = CBR_4800;		break;
			case 0x0D: // fall through [7200 bps]
			case 0x0E: m_uBaudRate = CBR_9600;		break;
			case 0x0F: m_uBaudRate = CBR_19200;		break;
		}

		if (m_uControlByte & 0x10)
		{
			// receiver clock source [0= external, 1= internal]
		}

		// UPDATE THE BYTE SIZE
		switch (m_uControlByte & 0x60)
		{
			case 0x00: m_uByteSize = 8; break;
			case 0x20: m_uByteSize = 7; break;
			case 0x40: m_uByteSize = 6; break;
			case 0x60: m_uByteSize = 5; break;
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

static UINT g_uDbgTotalSSCRx = 0;

BYTE __stdcall CSuperSerialCard::CommReceive(WORD, WORD, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return 0;

	BYTE result = 0;

	if (!m_qTcpSerialBuffer.empty())
	{
		result = m_qTcpSerialBuffer.front();
		m_qTcpSerialBuffer.pop_front();
	}
	else if (m_hCommHandle != INVALID_HANDLE_VALUE)	// COM
	{
		EnterCriticalSection(&m_CriticalSection);
		{
			const UINT uCOMIdx = m_vuRxCurrBuffer;
			const UINT uSSCIdx = uCOMIdx ^ 1;
			if (!m_qComSerialBuffer[uSSCIdx].empty())
			{
				result = m_qComSerialBuffer[uSSCIdx].front();
				m_qComSerialBuffer[uSSCIdx].pop_front();

				UINT uNewSSCIdx = uSSCIdx;
				if ( m_qComSerialBuffer[uSSCIdx].empty() &&		// Current SSC buffer is empty
					!m_qComSerialBuffer[uCOMIdx].empty() )		// Current COM buffer has data
				{
					m_vuRxCurrBuffer = uSSCIdx;				// Flip buffers
					uNewSSCIdx = uCOMIdx;
				}

				if (m_bRxIrqEnabled && !m_qComSerialBuffer[uNewSSCIdx].empty())
				{
					CpuIrqAssert(IS_SSC);
					m_vbRxIrqPending = true;
				}
			}
		}
		LeaveCriticalSection(&m_CriticalSection);

		g_uDbgTotalSSCRx++;
	}

	return result;
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::CommTransmit(WORD, WORD, BYTE, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (m_hCommAcceptSocket != INVALID_SOCKET)
	{
		BYTE data = value;
		if (m_uByteSize < 8)
		{
			data &= ~(1 << m_uByteSize);
		}
		send(m_hCommAcceptSocket, (const char*)&data, 1, 0);
		m_bWrittenTx = true;	// Transmit done
	}
	else if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		DWORD uBytesWritten;
		WriteFile(m_hCommHandle, &value, 1, &uBytesWritten, &m_o);
		m_bWrittenTx = true;	// Transmit done
	}

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

	bool bComSerialBufferEmpty = true;	// Assume true, so if using TCP then logic below works

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		EnterCriticalSection(&m_CriticalSection);
		const UINT uSSCIdx = m_vuRxCurrBuffer ^ 1;
		bComSerialBufferEmpty = m_qComSerialBuffer[uSSCIdx].empty();
	}

	bool bIRQ = false;
	if (m_bTxIrqEnabled && m_bWrittenTx)
	{
		bIRQ = true;
	}
	if (m_bRxIrqEnabled)
	{
		bIRQ = m_vbRxIrqPending;
		m_vbRxIrqPending = false;	// Ensure 2 reads of STATUS reg only return ST_IRQ for first read
	}

	m_bWrittenTx = false;		// Read status reg always clears IRQ

	//

	BYTE uStatus = ST_TX_EMPTY 
				| ((!bComSerialBufferEmpty || !m_qTcpSerialBuffer.empty()) ? ST_RX_FULL : 0x00)
#ifdef SUPPORT_MODEM
				| ((modemstatus & MS_RLSD_ON)	? 0x00 : ST_DCD)	// Need 0x00 to allow ZLink to start up
				| ((modemstatus & MS_DSR_ON)	? 0x00 : ST_DSR)
#endif
				| (bIRQ							? ST_IRQ : 0x00);

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		LeaveCriticalSection(&m_CriticalSection);
	}

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

	InternalReset();
}

//===========================================================================

void CSuperSerialCard::CommDestroy()
{
	CommReset();

	delete [] m_pExpansionRom;
	m_pExpansionRom = NULL;
}

//===========================================================================

// dwNewSerialPortItem is the drop-down list item
void CSuperSerialCard::CommSetSerialPort(HWND hWindow, DWORD dwNewSerialPortItem)
{
	if (m_dwSerialPortItem == dwNewSerialPortItem)
		return;

	_ASSERT(!IsActive());
	if (IsActive())
		return;

	m_dwSerialPortItem = dwNewSerialPortItem;

	if (m_dwSerialPortItem == m_uTCPChoiceItemIdx)
		strcpy(m_ayCurrentSerialPortName, TEXT_SERIAL_TCP);
	else if (m_dwSerialPortItem != 0)
		sprintf(m_ayCurrentSerialPortName, TEXT_SERIAL_COM"%d", m_vecSerialPortsItems[m_dwSerialPortItem]);
	else
		m_ayCurrentSerialPortName[0] = 0;	// "None"
}

//===========================================================================

void CSuperSerialCard::CommUpdate(DWORD totalcycles)
{
	if (!IsActive())
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

// Had this error when sizeof(m_RecvBuffer)==1 was used
// UPDATE: Fixed by using double-buffered queue
//
// ERROR_OPERATION_ABORTED: CE_RXOVER
//
// Config:
// . DOS Box (laptop) -> ZLink (PC)
// . Baud = 300/4800/9600/19200
// . InQueue size = 0x1000
// . AppleII speed = 1MHz/2MHz/Unthrottled
// . TYPE AW-PascalCrash.txt >COM7
// . NB. AW-PascalCrash.txt is 10020 bytes
//
// Error:
// . Always get ERROR_OPERATION_ABORTED after reading 0x555 total bytes
// . dwErrors = 1 (CE_RXOVER)
// . COMSTAT::InQueue = 0x1000
//

static UINT g_uDbgTotalCOMRx = 0;

void CSuperSerialCard::CheckCommEvent(DWORD dwEvtMask)
{
	if (dwEvtMask & EV_RXCHAR)
	{
		char Data[0x80];
		DWORD dwReceived = 0;
		bool bGotData = false;

		// Read COM buffer until empty
		// NB. Potentially dangerous, as Apple read rate might be too slow, so could run out of memory on PC!
		do
		{
			if (!ReadFile(m_hCommHandle, Data, sizeof(Data), &dwReceived, &m_o) || !dwReceived)
				break;

			g_uDbgTotalCOMRx += dwReceived;

			bGotData = true;

			EnterCriticalSection(&m_CriticalSection);
			{
				const UINT uCOMIdx = m_vuRxCurrBuffer;
				for (DWORD i = 0; i < dwReceived; i++)
					m_qComSerialBuffer[uCOMIdx].push_back(Data[i]);
			}
			LeaveCriticalSection(&m_CriticalSection);
		}
		while(sizeof(Data) == dwReceived);

		//

		if (bGotData)
		{
			EnterCriticalSection(&m_CriticalSection);
			{
				// NB. m_vuRxCurrBuffer may've changed since ReadFile() above -- can change in CommReceive()
				// - Maybe buffers have already been flipped

				const UINT uCOMIdx = m_vuRxCurrBuffer;
				const UINT uSSCIdx = uCOMIdx ^ 1;
				if ( m_qComSerialBuffer[uSSCIdx].empty() &&		// Current SSC buffer is empty
					!m_qComSerialBuffer[uCOMIdx].empty() )		// Current COM buffer has data
				{
					m_vuRxCurrBuffer = uSSCIdx;				// Flip buffers

					if (m_bRxIrqEnabled)
					{
						CpuIrqAssert(IS_SSC);
						m_vbRxIrqPending = true;
					}
				}
			}
			LeaveCriticalSection(&m_CriticalSection);
		}
	}
	//else if (dwEvtMask & EV_TXEMPTY)
	//{
	//	if (m_bTxIrqEnabled)
	//	{
	//		m_vbTxIrqPending = true;
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

	HANDLE hCommEvent_Wait[nNumEvents] = {pSSC->m_hCommEvent[COMMEVT_WAIT], pSSC->m_hCommEvent[COMMEVT_TERM]};
	HANDLE hCommEvent_Ack[nNumEvents]  = {pSSC->m_hCommEvent[COMMEVT_ACK],  pSSC->m_hCommEvent[COMMEVT_TERM]};

	while(1)
	{
		DWORD dwEvtMask = 0;
		DWORD dwWaitResult;

		bRes = WaitCommEvent(pSSC->m_hCommHandle, &dwEvtMask, &pSSC->m_o);	// Will return immediately (probably with ERROR_IO_PENDING)
		_ASSERT(!bRes);
		if (!bRes)
		{
			DWORD dwRet = GetLastError();
			_ASSERT(dwRet == ERROR_IO_PENDING);
			if (dwRet != ERROR_IO_PENDING)
			{
				// Probably: ERROR_OPERATION_ABORTED
				DWORD dwErrors;
				COMSTAT Stat;
				ClearCommError(pSSC->m_hCommHandle, &dwErrors, &Stat);
				if (dwErrors)
				{
					if (dwErrors & CE_RXOVER)
						sprintf(szDbg, "CommThread: Err=CE_RXOVER (0x%08X): InQueue=0x%08X\n", dwErrors, Stat.cbInQue);
					else
						sprintf(szDbg, "CommThread: Err=Other (0x%08X): InQueue=0x%08X, OutQueue=0x%08X\n", dwErrors, Stat.cbInQue, Stat.cbOutQue);
					OutputDebugString(szDbg);
					if (g_fh)
						fprintf(g_fh, szDbg);
				}
				return -1;
			}

			//
			// Wait for comm event
			//

			while(1)
			{
				//OutputDebugString("CommThread: Wait1\n");
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
			//sprintf(szDbg, "CommThread: GotEvent1: %d\n", dwWaitResult); OutputDebugString(szDbg);

			if (dwWaitResult == (nNumEvents-1))
				break;	// Termination event
		}

		// Comm event
		pSSC->CheckCommEvent(dwEvtMask);
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

		SetThreadPriority(m_hCommThread, THREAD_PRIORITY_TIME_CRITICAL);

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

void CSuperSerialCard::ScanCOMPorts()
{
	m_vecSerialPortsItems.clear();
	m_vecSerialPortsItems.push_back(SERIALPORTITEM_INVALID_COM_PORT);	// "None"

	for (UINT i=1; i<32; i++)	// Arbitrary upper limit
	{
		TCHAR portname[SIZEOF_SERIALCHOICE_ITEM];
		wsprintf(portname, TEXT("COM%u"), i);

		HANDLE hCommHandle = CreateFile(portname,
								GENERIC_READ | GENERIC_WRITE,
								0,								// exclusive access
								(LPSECURITY_ATTRIBUTES)NULL,	// default security attributes
								OPEN_EXISTING,
								FILE_FLAG_OVERLAPPED,			// required for WaitCommEvent()
								NULL);

		if (hCommHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(hCommHandle);
			m_vecSerialPortsItems.push_back(i);
		}
	}

	//

	m_vecSerialPortsItems.push_back(SERIALPORTITEM_INVALID_COM_PORT);	// "TCP"
	m_uTCPChoiceItemIdx = m_vecSerialPortsItems.size()-1;
}

char* CSuperSerialCard::GetSerialPortChoices()
{
	if (IsActive())
		return m_aySerialPortChoices;

	//

	ScanCOMPorts();				// Do this every time in case news ones available (eg. for USB COM ports)
	delete [] m_aySerialPortChoices;
	m_aySerialPortChoices = new TCHAR [ GetNumSerialPortChoices() * SIZEOF_SERIALCHOICE_ITEM + 1 ];	// +1 for final NULL item

	TCHAR* pNextSerialChoice = m_aySerialPortChoices;

	//

	pNextSerialChoice += wsprintf(pNextSerialChoice, TEXT("None"));
	pNextSerialChoice++;		// Skip NULL char

	for (UINT i=1; i<m_uTCPChoiceItemIdx; i++)
	{
		pNextSerialChoice += wsprintf(pNextSerialChoice, TEXT("COM%u"), m_vecSerialPortsItems[i]);
		pNextSerialChoice++;	// Skip NULL char
	}

	pNextSerialChoice += wsprintf(pNextSerialChoice, TEXT("TCP"));
	pNextSerialChoice++;		// Skip NULL char

	*pNextSerialChoice = 0;

	//

	return m_aySerialPortChoices;
}

// Called by LoadConfiguration()
void CSuperSerialCard::SetSerialPortName(const char* pSerialPortName)
{
	strncpy(m_ayCurrentSerialPortName, pSerialPortName, SIZEOF_SERIALCHOICE_ITEM);

	// Init m_aySerialPortChoices, so that we have choices to show if serial is active when we 1st open Config dialog
	GetSerialPortChoices();

	if (strncmp(TEXT_SERIAL_COM, pSerialPortName, sizeof(TEXT_SERIAL_COM)-1) == 0)
	{
		const char* p = &pSerialPortName[ sizeof(TEXT_SERIAL_COM)-1 ];
		const int nCOMPort = atoi(p);
		m_dwSerialPortItem = 0;
		for (UINT i=0; i<m_vecSerialPortsItems.size(); i++)
		{
			if (m_vecSerialPortsItems[i] == nCOMPort)
			{
				m_dwSerialPortItem = i;
				break;
			}
		}
		//_ASSERT(m_dwSerialPortItem);	// EG. Switched a USB COM port from COM7 to COM8 between AppleWin sessions

		if (m_dwSerialPortItem >= GetNumSerialPortChoices())
		{
			_ASSERT(0);
			m_dwSerialPortItem = 0;
		}
	}
	else if (strncmp(TEXT_SERIAL_TCP, pSerialPortName, sizeof(TEXT_SERIAL_TCP)-1) == 0)
	{
		m_dwSerialPortItem = m_uTCPChoiceItemIdx;
	}
	else
	{
		m_ayCurrentSerialPortName[0] = 0;	// "None"
		m_dwSerialPortItem = 0;
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
//	memcpy(pSS->recvbuffer, m_RecvBuffer, uRecvBufferSize);
	pSS->recvbytes		= 0;
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
//	memcpy(m_RecvBuffer, pSS->recvbuffer, uRecvBufferSize);
//	m_vRecvBytes	= pSS->recvbytes;
	m_uStopBits		= pSS->stopbits;
	return 0;
}
