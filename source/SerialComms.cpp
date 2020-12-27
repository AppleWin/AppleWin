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

// Refs:
// [Ref.1] AppleWin\docs\SSC Memory Locations for Programmers.txt
// [Ref.2] SSC recv IRQ example: https://sites.google.com/site/drjohnbmatthews/apple2/ssc - John B. Matthews, 5/13/87
// [Ref.3] SY6551 info: http://users.axess.com/twilight/sock/rs232pak.html
//
// SSC-pg is an abbreviation for pages references to "Super Serial Card, Installation and Operating Manual" by Apple

#include "StdAfx.h"

#include "SerialComms.h"
#include "CPU.h"
#include "Interface.h"
#include "Log.h"
#include "Memory.h"
#include "YamlHelper.h"

#include "../resource/resource.h"

#define TCP_SERIAL_PORT 1977

// Default: 9600-8-N-1
SSC_DIPSW CSuperSerialCard::m_DIPSWDefault =
{
	// DIPSW1:
	CBR_9600,		// Use 9600, as a 1MHz Apple II can only handle up to 9600 bps [Ref.1]
	FWMODE_CIC,

	// DIPSW2:
	ONESTOPBIT,
	8,				// ByteSize
	NOPARITY,
	false,			// SW2-5: LF(0x0A). SSC-24: In Comms mode, SSC automatically discards LF immediately following CR
	true,			// SW2-6: Interrupts. SSC-47: Passes interrupt requests from ACIA to the Apple II. NB. Can't be read from software
};

//===========================================================================

CSuperSerialCard::CSuperSerialCard(UINT slot) :
	Card(CT_SSC),
	m_uSlot(slot),
	m_aySerialPortChoices(NULL),
	m_uTCPChoiceItemIdx(0),
	m_bCfgSupportDCD(false),
	m_pExpansionRom(NULL)
{
	m_ayCurrentSerialPortName.clear();
	m_dwSerialPortItem = 0;

	m_hCommHandle = INVALID_HANDLE_VALUE;
	m_hCommListenSocket = INVALID_SOCKET;
	m_hCommAcceptSocket = INVALID_SOCKET;

	m_hCommThread = NULL;

	for (UINT i=0; i<COMMEVT_MAX; i++)
		m_hCommEvent[i] = NULL;

	memset(&m_o, 0, sizeof(m_o));

	InternalReset();
}

void CSuperSerialCard::InternalReset()
{
	GetDIPSW();

	// SY6551 datasheet: Hardware reset sets Command register to 0
	// . NB. MOS6551 datasheet: Hardware reset: b#00000010 (so ACIA not init'd on IN#2!)
	// SY6551 datasheet: Hardware reset sets Control register to 0 - the DIPSW settings are not used by h/w to setup this register
	UpdateCommandAndControlRegs(0, 0);	// Baud=External clock! 8-N-1

	//

	m_vbTxIrqPending = false;
	m_vbRxIrqPending = false;
	m_vbTxEmpty = true;

	m_vuRxCurrBuffer = 0;
	m_qComSerialBuffer[0].clear();
	m_qComSerialBuffer[1].clear();
	m_qTcpSerialBuffer.clear();

	m_uDTR = DTR_CONTROL_DISABLE;
	m_uRTS = RTS_CONTROL_DISABLE;
	m_dwModemStatus = m_kDefaultModemStatus;
}

CSuperSerialCard::~CSuperSerialCard()
{
	delete [] m_aySerialPortChoices;
}

//===========================================================================

// TODO: Serial Comms - UI Property Sheet Page:
// . Ability to config the 2x DIPSWs - only takes affect after next Apple2 reset
// . 'Default' button that resets DIPSWs to DIPSWDefaults
// . Must respect IRQ disable dipswitch (cannot be overridden or read by software)

void CSuperSerialCard::GetDIPSW()
{
	// TODO: Read settings from Registry(?). In the meantime, use the defaults:
	SetDIPSWDefaults();
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
	LogFileOutput("SSC: BaudRateToIndex(): unsupported rate: %d\n", uBaudRate);
	return BaudRateToIndex(m_kDefaultBaudRate);	// nominally use AppleWin default
}

//===========================================================================

void CSuperSerialCard::UpdateCommState()
{
	if (m_hCommHandle == INVALID_HANDLE_VALUE)
		return;

	DCB dcb;
	memset(&dcb, 0, sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	GetCommState(m_hCommHandle,&dcb);
	dcb.BaudRate = m_uBaudRate;
	dcb.ByteSize = m_uByteSize;
	dcb.Parity   = m_uParity;
	dcb.StopBits = m_uStopBits;

	// Specifies the DTR (data-terminal-ready) input flow control (use dcb.fOutxCtsFlow for output)
	dcb.fDtrControl = m_uDTR;	// GH#386

	// Specifies the RTS (request-to-send) input flow control (use dcb.fOutxDsrFlow for output)
	dcb.fRtsControl = m_uRTS;	// GH#311

	SetCommState(m_hCommHandle,&dcb);
}

//===========================================================================

bool CSuperSerialCard::CheckComm()
{
	// check for COM or TCP socket handle, and setup if invalid
	if (IsActive())
		return true;

	if (m_dwSerialPortItem == m_uTCPChoiceItemIdx)
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) // Winsock 2.2
		{
			if (wsaData.wVersion != 0x0202)
			{
				WSACleanup();
				return false;
			}

			// initialized, so try to create a socket
			m_hCommListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (m_hCommListenSocket == INVALID_SOCKET)
			{
				WSACleanup();
				return false;
			}

			// have socket so attempt to bind it
			SOCKADDR_IN saAddress;
			memset(&saAddress, 0, sizeof(SOCKADDR_IN));
			saAddress.sin_family = AF_INET;
			saAddress.sin_port = htons(TCP_SERIAL_PORT); // TODO: get from registry / GUI
			saAddress.sin_addr.s_addr = htonl(INADDR_ANY);
			if (bind(m_hCommListenSocket, (LPSOCKADDR)&saAddress, sizeof(saAddress)) == SOCKET_ERROR)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return false;
			}

			// bound, so listen
			if (listen(m_hCommListenSocket, 1) == SOCKET_ERROR)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return false;
			}

			// now send async events to our app's message handler
			if (WSAAsyncSelect(
					/* SOCKET s */ m_hCommListenSocket,
					/* HWND hWnd */ GetFrame().g_hFrameWindow,
					/* unsigned int wMsg */ WM_USER_TCP_SERIAL,
					/* long lEvent */ (FD_ACCEPT | FD_CONNECT | FD_READ | FD_CLOSE)) != 0)
			{
				m_hCommListenSocket = INVALID_SOCKET;
				WSACleanup();
				return false;
			}
		}
	}
	else if (m_dwSerialPortItem)
	{
		_ASSERT(m_dwSerialPortItem < m_vecSerialPortsItems.size()-1);	// size()-1 is TCP item
		TCHAR portname[SIZEOF_SERIALCHOICE_ITEM];
		wsprintf(portname, TEXT("\\\\.\\COM%u"), m_vecSerialPortsItems[m_dwSerialPortItem]);

		m_hCommHandle = CreateFile(portname,
								GENERIC_READ | GENERIC_WRITE,
								0,								// exclusive access
								(LPSECURITY_ATTRIBUTES)NULL,	// default security attributes
								OPEN_EXISTING,
								FILE_FLAG_OVERLAPPED,			// required for WaitCommEvent()
								NULL);

		if (m_hCommHandle != INVALID_HANDLE_VALUE)
		{
			GetCommModemStatus(m_hCommHandle, const_cast<DWORD*>(&m_dwModemStatus));

			//BOOL bRes = SetupComm(m_hCommHandle, 8192, 8192);
			//_ASSERT(bRes);

			UpdateCommState();

			// ReadIntervalTimeout=MAXDWORD; ReadTotalTimeoutConstant=ReadTotalTimeoutMultiplier=0:
			// Read operation is to return immediately with the bytes that have already been received,
			// even if no bytes have been received.
			COMMTIMEOUTS ct;
			memset(&ct, 0, sizeof(COMMTIMEOUTS));
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
}

//===========================================================================

void CSuperSerialCard::CommTcpSerialCleanup()
{
	if (m_hCommListenSocket != INVALID_SOCKET)
	{
		WSAAsyncSelect(m_hCommListenSocket, GetFrame().g_hFrameWindow, 0, 0); // Stop event messages
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
		m_hCommAcceptSocket = accept(m_hCommListenSocket, NULL, NULL);
	}
}

//===========================================================================

// Called when there's a TCP event via the message pump
// . Because it's via the message pump, then this call is synchronous to CommReceive(), so there's no need for a critical section
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
			m_vbRxIrqPending = true;
		}
	}
}

//===========================================================================

BYTE __stdcall CSuperSerialCard::SSC_IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CSuperSerialCard* pSSC = (CSuperSerialCard*) MemGetSlotParameters(uSlot);

	switch (uAddr & 0xf)
	{
	case 0x0:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x1:	return pSSC->CommDipSw(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x2:	return pSSC->CommDipSw(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x3:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x4:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x5:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x6:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x7:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x8:	return pSSC->CommReceive(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x9:	return pSSC->CommStatus(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xA:	return pSSC->CommCommand(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xB:	return pSSC->CommControl(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xC:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xD:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xE:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xF:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	}

	return 0;
}

BYTE __stdcall CSuperSerialCard::SSC_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CSuperSerialCard* pSSC = (CSuperSerialCard*) MemGetSlotParameters(uSlot);

	switch (uAddr & 0xf)
	{
	case 0x0:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x1:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x2:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x3:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x4:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x5:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x6:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x7:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x8:	return pSSC->CommTransmit(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0x9:	return pSSC->CommProgramReset(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xA:	return pSSC->CommCommand(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xB:	return pSSC->CommControl(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xC:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xD:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xE:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	case 0xF:	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
	}

	return 0;
}

//===========================================================================

// 6551 ACIA Command Register ($C08A+s0)
// . EG. 0x09 = "no parity, enable IRQ" [Ref.2] - b7:5(No parity), b4 (No echo), b3:1(Enable TX,RX IRQs), b0(DTR: Enable receiver and all interrupts)
enum {	CMD_PARITY_MASK				= 3<<6,
		CMD_PARITY_ODD				= 0<<6,		// Odd parity
		CMD_PARITY_EVEN				= 1<<6,		// Even parity
		CMD_PARITY_MARK				= 2<<6,		// Mark parity
		CMD_PARITY_SPACE			= 3<<6,		// Space parity
		CMD_PARITY_ENA				= 1<<5,
		CMD_ECHO_MODE				= 1<<4,
		CMD_TX_MASK					= 3<<2,
		CMD_TX_IRQ_DIS_RTS_HIGH		= 0<<2,
		CMD_TX_IRQ_ENA_RTS_LOW		= 1<<2,
		CMD_TX_IRQ_DIS_RTS_LOW		= 2<<2,
		CMD_TX_IRQ_DIS_RTS_LOW_BRK	= 3<<2,		// Transmit BRK
		CMD_RX_IRQ_DIS				= 1<<1,		// 1=IRQ interrupt disabled
		CMD_DTR						= 1<<0,		// Data Terminal Ready: Enable(1) or disable(0) receiver and all interrupts (!DTR low)
};

BYTE __stdcall CSuperSerialCard::CommProgramReset(WORD, WORD, BYTE, BYTE, ULONG)
{
	// Command: top-3 parity bits unaffected
	UpdateCommandReg( m_uCommandByte & (CMD_PARITY_MASK|CMD_PARITY_ENA) );

	// Control: all bits unaffected
	// Status: all bits unaffects, except Overrun(bit2) is cleared

	return 0;
}

//===========================================================================

void CSuperSerialCard::UpdateCommandAndControlRegs(BYTE uCommandByte, BYTE uControlByte)
{
	// UpdateCommandReg() first to initialise m_uParity, before calling UpdateControlReg()
	UpdateCommandReg(uCommandByte);
	UpdateControlReg(uControlByte);
}

void CSuperSerialCard::UpdateCommandReg(BYTE command)
{
	m_uCommandByte = command;

	if (m_uCommandByte & CMD_PARITY_ENA)
	{
		switch (m_uCommandByte & CMD_PARITY_MASK)
		{
		case CMD_PARITY_ODD:	m_uParity = ODDPARITY; break;
		case CMD_PARITY_EVEN:	m_uParity = EVENPARITY; break;
		case CMD_PARITY_MARK:	m_uParity = MARKPARITY; break;
		case CMD_PARITY_SPACE:	m_uParity = SPACEPARITY; break;
		}
	}
	else
	{
		m_uParity = NOPARITY;
	}

	if (m_uCommandByte & CMD_ECHO_MODE)		// Receiver mode echo (0=no echo, 1=echo)
	{
		_ASSERT(0);
		LogFileOutput("SSC: CommCommand(): unsupported Echo mode. Command=0x%02X\n", m_uCommandByte);
	}

	switch (m_uCommandByte & CMD_TX_MASK)	// transmitter interrupt control
	{
		// Note: the RTS signal must be set 'low' in order to receive any incoming data from the serial device [Ref.1]
		case CMD_TX_IRQ_DIS_RTS_HIGH:		// set RTS high and transmit no interrupts (transmitter is off [Ref.3])
			m_uRTS = RTS_CONTROL_DISABLE;
			break;
		case CMD_TX_IRQ_ENA_RTS_LOW:		// set RTS low and transmit interrupts
			m_uRTS = RTS_CONTROL_ENABLE;
			break;
		case CMD_TX_IRQ_DIS_RTS_LOW:		// set RTS low and transmit no interrupts
			m_uRTS = RTS_CONTROL_ENABLE;
			break;
		case CMD_TX_IRQ_DIS_RTS_LOW_BRK:	// set RTS low and transmit break signals instead of interrupts
			m_uRTS = RTS_CONTROL_ENABLE;
			_ASSERT(0);
			LogFileOutput("SSC: CommCommand(): unsupported TX mode. Command=0x%02X\n", m_uCommandByte);
			break;
	}

	if (m_DIPSWCurrent.bInterrupts && m_uCommandByte & CMD_DTR)
	{
		// Assume enabling Rx IRQ if STATUS.ST_RX_FULL *does not* trigger an IRQ
		// . EG. Disable Rx IRQ, receive a byte (don't read STATUS or RX_DATA register), enable Rx IRQ
		// Assume enabling Tx IRQ if STATUS.ST_TX_EMPTY *does not* trigger an IRQ
		// . otherwise there'd be a "false" TX Empty IRQ even if nothing had ever been transferred!
		m_bTxIrqEnabled = (m_uCommandByte & CMD_TX_MASK) == CMD_TX_IRQ_ENA_RTS_LOW;
		m_bRxIrqEnabled = (m_uCommandByte & CMD_RX_IRQ_DIS) == 0;
	}
	else
	{
		m_bTxIrqEnabled = false;
		m_bRxIrqEnabled = false;
	}

	// Data Terminal Ready (DTR) setting (0=set DTR high (indicates 'not ready')) (GH#386)
	m_uDTR = (m_uCommandByte & CMD_DTR) ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
}

BYTE __stdcall CSuperSerialCard::CommCommand(WORD, WORD, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value	!= m_uCommandByte))
	{
		UpdateCommandReg(value);
		UpdateCommState();
	}

	return m_uCommandByte;
}

//===========================================================================

void CSuperSerialCard::UpdateControlReg(BYTE control)
{
	m_uControlByte = control;

	// UPDATE THE BAUD RATE
	switch (m_uControlByte & 0x0F)
	{
		// Note that 1 MHz Apples (everything other than the Apple IIgs and //c
		// Plus running in "fast" mode) cannot handle 19.2 kbps, and even 9600
		// bps on these machines requires either some highly optimised code or
		// a decent buffer in the device being accessed.  The faster Apples
		// have no difficulty with this speed, however. [Ref.1]

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
}

BYTE __stdcall CSuperSerialCard::CommControl(WORD, WORD, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value != m_uControlByte))
	{
		UpdateControlReg(value);
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

	if (!m_qTcpSerialBuffer.empty())
	{
		// NB. See CommTcpSerialReceive() above, for a note explaining why there's no need for a critical section here

		// If receiver is disabled then transmitting device should not send data
		// . For COM serial connection this is handled by DTR/DTS flow-control (which enables the receiver)
		if ((m_uCommandByte & CMD_DTR) == 0)	// Receiver disable, so prevent receiving data
			return 0;

		result = m_qTcpSerialBuffer.front();
		m_qTcpSerialBuffer.pop_front();

		if (m_bRxIrqEnabled && !m_qTcpSerialBuffer.empty())
		{
			CpuIrqAssert(IS_SSC);
			m_vbRxIrqPending = true;
		}
	}
	else if (m_hCommHandle != INVALID_HANDLE_VALUE)
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
	}

	return result;
}

//===========================================================================

void CSuperSerialCard::TransmitDone(void)
{
	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		// Use CriticalSection to ensure that write to m_vbTxEmpty is atomic w.r.t CommTransmit() (GH#707)
		EnterCriticalSection(&m_CriticalSection);

		_ASSERT(m_vbTxEmpty == false);
		m_vbTxEmpty = true;	// Transmit done (COM)

		LeaveCriticalSection(&m_CriticalSection);
	}
	else
	{
		_ASSERT(m_vbTxEmpty == false);
		m_vbTxEmpty = true;	// Transmit done (TCP)
	}

	if (m_bTxIrqEnabled)	// GH#522
	{
		CpuIrqAssert(IS_SSC);
		m_vbTxIrqPending = true;
	}
}

BYTE __stdcall CSuperSerialCard::CommTransmit(WORD, WORD, BYTE, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	// If transmitter is disabled then: Is data just discarded or does it get transmitted if transmitter is later enabled?
	if ((m_uCommandByte & CMD_TX_MASK) == CMD_TX_IRQ_DIS_RTS_HIGH)	// Transmitter disable, so just discard for now
		return 0;

	if (m_hCommAcceptSocket != INVALID_SOCKET)
	{
		BYTE data = value;
		if (m_uByteSize < 8)
		{
			data &= ~(1 << m_uByteSize);
		}
		int sent = send(m_hCommAcceptSocket, (const char*)&data, 1, 0);
		_ASSERT(sent == 1);
		if (sent == 1)
		{
			m_vbTxEmpty = false;
			// Assume that send() completes immediately
			TransmitDone();
		}
	}
	else if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		BOOL res = false;
		DWORD error = 0;

		// Use CriticalSection to keep WriteFile() & m_vbTxEmpty in sync (GH#707)
		EnterCriticalSection(&m_CriticalSection);

		_ASSERT(m_vbTxEmpty == true);
		DWORD uBytesWritten;
		res = WriteFile(m_hCommHandle, &value, 1, &uBytesWritten, &m_o);
		_ASSERT(res);
		if (res)
		{
			m_vbTxEmpty = false;
			// NB. Now CommThread() determines when transmit buffer is empty and calls TransmitDone()
		}
		else
		{
			error = GetLastError();
		}

		LeaveCriticalSection(&m_CriticalSection);

		if (!res)
			LogFileOutput("SSC: CommTransmit(): WriteFile() failed: 0x%08X\n", error);
	}

	return 0;
}

//===========================================================================

// 6551 ACIA Status Register ($C089+s0)
// ------------------------------------
// Bit 	Value 	Meaning
// 7    1       Interrupt (IRQ) true (cleared by reading status reg [Ref.3])
// 6    0       Data Set Ready (DSR) true [0=DSR low (ready), 1=DSR high (not ready)]
// 5    0       Data Carrier Detect (DCD) true [0=DCD low (detected), 1=DCD high (not detected)]
// 4    1       Transmit register empty
// 3    1       Receive register full
// 2    1       Overrun error
// 1    1       Framing error
// 0    1       Parity error

enum {	ST_IRQ			= 1<<7,
		ST_DSR			= 1<<6,
		ST_DCD			= 1<<5,
		ST_TX_EMPTY		= 1<<4,
		ST_RX_FULL		= 1<<3,
		ST_OVERRUN_ERR	= 1<<2,
		ST_FRAMING_ERR	= 1<<1,
		ST_PARITY_ERR	= 1<<0,
};

BYTE __stdcall CSuperSerialCard::CommStatus(WORD, WORD, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return ST_DSR | ST_DCD | ST_TX_EMPTY;

	DWORD modemStatus = m_kDefaultModemStatus;
	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		modemStatus = m_dwModemStatus;	// Take a copy of this volatile variable
		if (!m_bCfgSupportDCD)			// Default: DSR state is mirrored to DCD (GH#553)
		{
			modemStatus &= ~MS_RLSD_ON;
			if (modemStatus & MS_DSR_ON)
				modemStatus |= MS_RLSD_ON;
		}
	}
	else if (m_hCommListenSocket != INVALID_SOCKET && m_hCommAcceptSocket != INVALID_SOCKET)
	{
		modemStatus = MS_RLSD_ON | MS_DSR_ON | MS_CTS_ON;
	}

	//

	bool bComSerialBufferEmpty = true;	// Assume true, so if using TCP then logic below works

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		EnterCriticalSection(&m_CriticalSection);
		const UINT uSSCIdx = m_vuRxCurrBuffer ^ 1;
		bComSerialBufferEmpty = m_qComSerialBuffer[uSSCIdx].empty();
	}

	BYTE IRQ = 0;
	if (m_bTxIrqEnabled)
	{
		IRQ |= m_vbTxIrqPending ? ST_IRQ : 0;
		m_vbTxIrqPending = false;	// Ensure 2 reads of STATUS reg only return ST_IRQ for first read
	}
	if (m_bRxIrqEnabled)
	{
		IRQ |= m_vbRxIrqPending ? ST_IRQ : 0;
		m_vbRxIrqPending = false;	// Ensure 2 reads of STATUS reg only return ST_IRQ for first read
	}

	//

	BYTE DSR = (modemStatus & MS_DSR_ON)  ? 0x00 : ST_DSR;	// DSR is active low (see SY6551 datasheet) (GH#386)
	BYTE DCD = (modemStatus & MS_RLSD_ON) ? 0x00 : ST_DCD;	// DCD is active low (see SY6551 datasheet) (GH#386)

	//

	BYTE TX_EMPTY = m_vbTxEmpty ? ST_TX_EMPTY : 0;
	BYTE RX_FULL  = (!bComSerialBufferEmpty || !m_qTcpSerialBuffer.empty()) ? ST_RX_FULL : 0;

	//

	BYTE uStatus =
		  IRQ
		| DSR
		| DCD
		| TX_EMPTY
		| RX_FULL;

	if (m_hCommHandle != INVALID_HANDLE_VALUE)
	{
		LeaveCriticalSection(&m_CriticalSection);
	}

	CpuIrqDeassert(IS_SSC);		// Read status reg always clears IRQ

	return uStatus;
}

//===========================================================================

// NB. Some DIPSW settings can't be read:
// SSC-47: Three switches are not connected to the LS365s:
// . SW2-6: passes interrupt requests from ACIA to the Apple II
// . SW1-7 ON and SW2-7 OFF: connects DCD to the DCD input of the ACIA
// . SW1-7 OFF and SW2-7 ON: splices SCTS to the DCD input of the ACIA (when jumper is in TERMINAL position)
BYTE __stdcall CSuperSerialCard::CommDipSw(WORD, WORD addr, BYTE, BYTE, ULONG)
{
	BYTE sw = 0;

	switch (addr & 0xf)
	{
	case 1:	// DIPSW1
		sw = (BaudRateToIndex(m_DIPSWCurrent.uBaudRate)<<4) | m_DIPSWCurrent.eFirmwareMode;
		break;

	case 2:	// DIPSW2
		// Comms mode: SSC-23
		BYTE SW2_1 = m_DIPSWCurrent.uStopBits == TWOSTOPBITS ? 1 : 0;	// SW2-1 (Stop bits: 1-ON(0); 2-OFF(1))
		BYTE SW2_2 = m_DIPSWCurrent.uByteSize == 7 ? 1 : 0;				// SW2-2 (Data bits: 8-ON(0); 7-OFF(1))

		// SW2-3 (Parity: odd-ON(0); even-OFF(1))
		// SW2-4 (Parity: none-ON(0); SW2-3 don't care)
		BYTE SW2_3,SW2_4;
		switch (m_DIPSWCurrent.uParity)
		{
		case ODDPARITY:
			SW2_3 = 0; SW2_4 = 1;
			break;
		case EVENPARITY:
			SW2_3 = 1; SW2_4 = 1;
			break;
		default:
			_ASSERT(0);
			// fall through...
		case NOPARITY:
			SW2_3 = 0; SW2_4 = 0;
			break;
		}

		BYTE SW2_5 = m_DIPSWCurrent.bLinefeed ? 0 : 1;					// SW2-5 (LF: yes-ON(0); no-OFF(1))

		BYTE CTS = 1;	// Default to CTS being false. (Support CTS in DIPSW: GH#311)
		if (CheckComm() && m_hCommHandle != INVALID_HANDLE_VALUE)
			CTS = (m_dwModemStatus & MS_CTS_ON) ? 0 : 1;	// CTS active low (see SY6551 datasheet)
		else if (m_hCommListenSocket != INVALID_SOCKET)
			CTS = (m_hCommAcceptSocket != INVALID_SOCKET) ? 0 : 1;

		// SSC-54:
		sw =	SW2_1<<7 |	// b7 : SW2-1
				    0<<6 |	// b6 : -
				SW2_2<<5 |	// b5 : SW2-2
				    0<<4 |	// b4 : -
				SW2_3<<3 |	// b3 : SW2-3
				SW2_4<<2 |	// b2 : SW2-4
				SW2_5<<1 |	// b1 : SW2-5
				  CTS<<0;	// b0 : CTS
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

	_ASSERT(m_uSlot == uSlot);
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
		m_ayCurrentSerialPortName = TEXT_SERIAL_TCP;
	else if (m_dwSerialPortItem != 0) {
		TCHAR temp[SIZEOF_SERIALCHOICE_ITEM];
		sprintf(temp, TEXT_SERIAL_COM"%d", m_vecSerialPortsItems[m_dwSerialPortItem]);
		m_ayCurrentSerialPortName = temp;
	}
	else
		m_ayCurrentSerialPortName.clear();	// "None"
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

	if (dwEvtMask & EV_TXEMPTY)
	{
		TransmitDone();
	}

	if (dwEvtMask & (EV_RLSD|EV_DSR|EV_CTS))
	{
		// For Win7-64: takes 1-2msecs!
		// Don't read from main emulation thread, otherwise a tight 6502 polling loop can kill emulator performance!
		GetCommModemStatus(m_hCommHandle, const_cast<DWORD*>(&m_dwModemStatus));
	}
}

DWORD WINAPI CSuperSerialCard::CommThread(LPVOID lpParameter)
{
	CSuperSerialCard* pSSC = (CSuperSerialCard*) lpParameter;
	char szDbg[100];

	BOOL bRes = SetCommMask(pSSC->m_hCommHandle, EV_RLSD | EV_DSR | EV_CTS | EV_TXEMPTY | EV_RXCHAR);
	if (!bRes)
	{
		sprintf(szDbg, "SSC: CommThread(): SetCommMask() failed\n");
		LogOutput("%s", szDbg);
		LogFileOutput("%s", szDbg);
		return -1;
	}

	//

	const UINT nNumEvents = 2;
	HANDLE hCommEvent_Wait[nNumEvents] = {pSSC->m_hCommEvent[COMMEVT_WAIT], pSSC->m_hCommEvent[COMMEVT_TERM]};

	while(1)
	{
		DWORD dwEvtMask = 0;
		DWORD dwWaitResult;

		bRes = WaitCommEvent(pSSC->m_hCommHandle, &dwEvtMask, &pSSC->m_o);	// Will return immediately (probably with ERROR_IO_PENDING)
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
				if (dwErrors & CE_RXOVER)
					sprintf(szDbg, "SSC: CommThread(): LastError=0x%08X, CommError=CE_RXOVER (0x%08X): InQueue=0x%08X\n", dwRet, dwErrors, Stat.cbInQue);
				else
					sprintf(szDbg, "SSC: CommThread(): LastError=0x%08X, CommError=Other (0x%08X): InQueue=0x%08X, OutQueue=0x%08X\n", dwRet, dwErrors, Stat.cbInQue, Stat.cbOutQue);
				LogOutput("%s", szDbg);
				LogFileOutput("%s", szDbg);
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
		InitializeCriticalSection(&m_CriticalSection);

		DWORD dwThreadId;
		m_hCommThread = CreateThread(NULL,			// lpThreadAttributes
									0,				// dwStackSize
									(LPTHREAD_START_ROUTINE) &CSuperSerialCard::CommThread,
									this,			// lpParameter
									0,				// dwCreationFlags : 0 = Run immediately
									&dwThreadId);	// lpThreadId

		SetThreadPriority(m_hCommThread, THREAD_PRIORITY_TIME_CRITICAL);
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
		wsprintf(portname, TEXT("\\\\.\\COM%u"), i);

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
	m_ayCurrentSerialPortName = pSerialPortName;

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
		m_ayCurrentSerialPortName.clear();	// "None"
		m_dwSerialPortItem = 0;
	}
}

//===========================================================================

// Unit version history:
// 2: Added: Support DCD flag
//    Removed: redundant data (encapsulated in Command & Control bytes)
static const UINT kUNIT_VERSION = 2;

#define SS_YAML_VALUE_CARD_SSC "Super Serial Card"

#define SS_YAML_KEY_DIPSWDEFAULT "DIPSW Default"
#define SS_YAML_KEY_DIPSWCURRENT "DIPSW Current"

#define SS_YAML_KEY_BAUDRATE "Baud Rate"
#define SS_YAML_KEY_FWMODE "Firmware mode"
#define SS_YAML_KEY_STOPBITS "Stop Bits"
#define SS_YAML_KEY_BYTESIZE "Byte Size"
#define SS_YAML_KEY_PARITY "Parity"
#define SS_YAML_KEY_LINEFEED "Linefeed"
#define SS_YAML_KEY_INTERRUPTS "Interrupts"
#define SS_YAML_KEY_CONTROL "Control Byte"
#define SS_YAML_KEY_COMMAND "Command Byte"
#define SS_YAML_KEY_INACTIVITY "Comm Inactivity"
#define SS_YAML_KEY_TXIRQENABLED "TX IRQ Enabled"
#define SS_YAML_KEY_RXIRQENABLED "RX IRQ Enabled"
#define SS_YAML_KEY_TXIRQPENDING "TX IRQ Pending"
#define SS_YAML_KEY_RXIRQPENDING "RX IRQ Pending"
#define SS_YAML_KEY_WRITTENTX "Written TX"
#define SS_YAML_KEY_SERIALPORTNAME "Serial Port Name"
#define SS_YAML_KEY_SUPPORT_DCD "Support DCD"

std::string CSuperSerialCard::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_SSC);
	return name;
}

void CSuperSerialCard::SaveSnapshotDIPSW(YamlSaveHelper& yamlSaveHelper, std::string key, SSC_DIPSW& dipsw)
{
	YamlSaveHelper::Label label(yamlSaveHelper, "%s:\n", key.c_str());
	yamlSaveHelper.SaveUint(SS_YAML_KEY_BAUDRATE, dipsw.uBaudRate);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_FWMODE, dipsw.eFirmwareMode);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_STOPBITS, dipsw.uStopBits);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_BYTESIZE, dipsw.uByteSize);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_PARITY, dipsw.uParity);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_LINEFEED, dipsw.bLinefeed);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_INTERRUPTS, dipsw.bInterrupts);
}

void CSuperSerialCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_uSlot, kUNIT_VERSION);

	YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	SaveSnapshotDIPSW(yamlSaveHelper, SS_YAML_KEY_DIPSWDEFAULT, m_DIPSWDefault);
	SaveSnapshotDIPSW(yamlSaveHelper, SS_YAML_KEY_DIPSWCURRENT, m_DIPSWCurrent);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_CONTROL, m_uControlByte);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_COMMAND, m_uCommandByte);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_TXIRQPENDING, m_vbTxIrqPending);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_RXIRQPENDING, m_vbRxIrqPending);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_WRITTENTX, m_vbTxEmpty);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_SUPPORT_DCD, m_bCfgSupportDCD);
	yamlSaveHelper.SaveString(SS_YAML_KEY_SERIALPORTNAME, GetSerialPortName());
}

void CSuperSerialCard::LoadSnapshotDIPSW(YamlLoadHelper& yamlLoadHelper, std::string key, SSC_DIPSW& dipsw)
{
	if (!yamlLoadHelper.GetSubMap(key))
		throw std::string("Card: Expected key: " + key);

	dipsw.uBaudRate		= yamlLoadHelper.LoadUint(SS_YAML_KEY_BAUDRATE);
	dipsw.eFirmwareMode = (eFWMODE) yamlLoadHelper.LoadUint(SS_YAML_KEY_FWMODE);
	dipsw.uStopBits		= yamlLoadHelper.LoadUint(SS_YAML_KEY_STOPBITS);
	dipsw.uByteSize		= yamlLoadHelper.LoadUint(SS_YAML_KEY_BYTESIZE);
	dipsw.uParity		= yamlLoadHelper.LoadUint(SS_YAML_KEY_PARITY);
	dipsw.bLinefeed		= yamlLoadHelper.LoadBool(SS_YAML_KEY_LINEFEED);
	dipsw.bInterrupts	= yamlLoadHelper.LoadBool(SS_YAML_KEY_INTERRUPTS);

	yamlLoadHelper.PopMap();
}

bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != 2)	// fixme
		throw std::string("Card: wrong slot");

	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	LoadSnapshotDIPSW(yamlLoadHelper, SS_YAML_KEY_DIPSWDEFAULT, m_DIPSWDefault);
	LoadSnapshotDIPSW(yamlLoadHelper, SS_YAML_KEY_DIPSWCURRENT, m_DIPSWCurrent);

	if (version == 1)	// Consume redundant/obsolete data
	{
		yamlLoadHelper.LoadUint(SS_YAML_KEY_PARITY);		// Redundant: derived from uCommandByte in UpdateCommandReg()
		yamlLoadHelper.LoadBool(SS_YAML_KEY_TXIRQENABLED);	// Redundant: derived from uCommandByte in UpdateCommandReg()
		yamlLoadHelper.LoadBool(SS_YAML_KEY_RXIRQENABLED);	// Redundant: derived from uCommandByte in UpdateCommandReg()

		yamlLoadHelper.LoadUint(SS_YAML_KEY_BAUDRATE);		// Redundant: derived from uControlByte in UpdateControlReg()
		yamlLoadHelper.LoadUint(SS_YAML_KEY_STOPBITS);		// Redundant: derived from uControlByte in UpdateControlReg()
		yamlLoadHelper.LoadUint(SS_YAML_KEY_BYTESIZE);		// Redundant: derived from uControlByte in UpdateControlReg()

		yamlLoadHelper.LoadUint(SS_YAML_KEY_INACTIVITY);	// Obsolete (so just consume)
	}
	else if (version >= 2)
	{
		SupportDCD( yamlLoadHelper.LoadBool(SS_YAML_KEY_SUPPORT_DCD) );
	}

	UINT uCommandByte	= yamlLoadHelper.LoadUint(SS_YAML_KEY_COMMAND);
	UINT uControlByte	= yamlLoadHelper.LoadUint(SS_YAML_KEY_CONTROL);
	UpdateCommandAndControlRegs(uCommandByte, uControlByte);

	m_vbTxIrqPending	= yamlLoadHelper.LoadBool(SS_YAML_KEY_TXIRQPENDING);
	m_vbRxIrqPending	= yamlLoadHelper.LoadBool(SS_YAML_KEY_RXIRQPENDING);
	m_vbTxEmpty			= yamlLoadHelper.LoadBool(SS_YAML_KEY_WRITTENTX);

	if (m_vbTxIrqPending || m_vbRxIrqPending)	// GH#677
		CpuIrqAssert(IS_SSC);

	std::string serialPortName = yamlLoadHelper.LoadString(SS_YAML_KEY_SERIALPORTNAME);
	SetSerialPortName(serialPortName.c_str());

	return true;
}
