/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

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

//#define SUPPORT_MODEM

static DWORD  baudrate       = CBR_19200;
static BYTE   bytesize       = 8;
static BYTE   commandbyte    = 0x00;
static HANDLE commhandle     = INVALID_HANDLE_VALUE;
static DWORD  comminactivity = 0;
static BYTE   controlbyte    = 0x1F;
static BYTE   parity         = NOPARITY;
DWORD  serialport     = 0;
static BYTE   stopbits       = ONESTOPBIT;

//

static CRITICAL_SECTION	g_CriticalSection;	// To guard /g_vRecvBytes/
static BYTE				g_RecvBuffer[uRecvBufferSize];	// NB: More work required if >1 is used
static volatile DWORD	g_vRecvBytes = 0;

//

static bool g_bTxIrqEnabled = false;
static bool g_bRxIrqEnabled = false;

static bool g_bWrittenTx = false;

//

static volatile bool g_vbCommIRQ = false;
static HANDLE g_hCommThread = NULL;

enum {COMMEVT_WAIT=0, COMMEVT_ACK, COMMEVT_TERM, COMMEVT_MAX};
static HANDLE g_hCommEvent[COMMEVT_MAX] = {NULL};
static OVERLAPPED o;

//===========================================================================

static void UpdateCommState ()
{
	if (commhandle == INVALID_HANDLE_VALUE)
		return;

	DCB dcb;
	ZeroMemory(&dcb,sizeof(DCB));
	dcb.DCBlength = sizeof(DCB);
	GetCommState(commhandle,&dcb);
	dcb.BaudRate = baudrate;
	dcb.ByteSize = bytesize;
	dcb.Parity   = parity;
	dcb.StopBits = stopbits;

	SetCommState(commhandle,&dcb);
}

//===========================================================================

static BOOL CheckComm ()
{
	comminactivity = 0;

	if ((commhandle == INVALID_HANDLE_VALUE) && serialport)
	{
		TCHAR portname[8];
		wsprintf(portname, TEXT("COM%u"), serialport);

		commhandle = CreateFile(portname,
								GENERIC_READ | GENERIC_WRITE,
								0,								// exclusive access
								(LPSECURITY_ATTRIBUTES)NULL,	// default security attributes
								OPEN_EXISTING,
								FILE_FLAG_OVERLAPPED,			// required for WaitCommEvent()
								NULL);

		if (commhandle != INVALID_HANDLE_VALUE)
		{
			UpdateCommState();
			COMMTIMEOUTS ct;
			ZeroMemory(&ct,sizeof(COMMTIMEOUTS));
			ct.ReadIntervalTimeout = MAXDWORD;
			SetCommTimeouts(commhandle,&ct);
			CommThInit();
		}
		else
		{
			DWORD uError = GetLastError();
		}
	}

	return (commhandle != INVALID_HANDLE_VALUE);
}

//===========================================================================

static void CloseComm ()
{
	CommThUninit();		// Kill CommThread before closing COM handle

	if (commhandle != INVALID_HANDLE_VALUE)
		CloseHandle(commhandle);

	commhandle = INVALID_HANDLE_VALUE;
	comminactivity = 0;
}

//===========================================================================

// EG. 0x09 = Enable IRQ, No parity [Ref.2]

BYTE __stdcall CommCommand (WORD, BYTE, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value	!= commandbyte))
	{
		commandbyte	= value;

		// UPDATE THE PARITY
		if (commandbyte	& 0x20)
		{
			switch (commandbyte	& 0xC0)
			{
			case 0x00 :	parity = ODDPARITY;	   break;
			case 0x40 :	parity = EVENPARITY;   break;
			case 0x80 :	parity = MARKPARITY;   break;
			case 0xC0 :	parity = SPACEPARITY;  break;
			}
		}
		else
		{
			parity = NOPARITY;
		}

		if (commandbyte	& 0x10)	// Receiver mode echo [0=no echo, 1=echo]
		{
		}

		switch (commandbyte	& 0x0C)	// transmitter interrupt control
		{
			// Note: the RTS signal must be set 'low' in order to receive any
			// incoming data from the serial device
			case 0<<2: // set RTS high and transmit no interrupts
				g_bTxIrqEnabled = false;
				break;
			case 1<<2: // set RTS low and transmit interrupts
				g_bTxIrqEnabled = true;
				break;
			case 2<<2: // set RTS low and transmit no interrupts
				g_bTxIrqEnabled = false;
				break;
			case 3<<2: // set RTS low and transmit break signals instead of interrupts
				g_bTxIrqEnabled = false;
				break;
		}

		// interrupt request disable [0=enable receiver interrupts]
		g_bRxIrqEnabled = ((commandbyte & 0x02) == 0);

		if (commandbyte	& 0x01)	// Data Terminal Ready (DTR) setting [0=set DTR high (indicates 'not ready')]
		{
			// Note that, although the DTR is generally not used in the SSC (it may actually not
			// be connected!), it must be set to 'low' in order for the 6551 to function correctly.
		}

		UpdateCommState();
	}

	return commandbyte;
}

//===========================================================================

BYTE __stdcall CommControl (WORD, BYTE, BYTE write, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	if (write && (value != controlbyte))
	{
		controlbyte = value;

		// UPDATE THE BAUD RATE
		switch (controlbyte & 0x0F)
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
			case 0x05: baudrate = CBR_110;     break;	// [150 bps]
			case 0x06: baudrate = CBR_300;     break;
			case 0x07: baudrate = CBR_600;     break;
			case 0x08: baudrate = CBR_1200;    break;
			case 0x09: // fall through [1800 bps]
			case 0x0A: baudrate = CBR_2400;    break;
			case 0x0B: // fall through [3600 bps]
			case 0x0C: baudrate = CBR_4800;    break;
			case 0x0D: // fall through [7200 bps]
			case 0x0E: baudrate = CBR_9600;    break;
			case 0x0F: baudrate = CBR_19200;   break;
		}

		if (controlbyte & 0x10)
		{
			// receiver clock source [0= external, 1= internal]
		}

		// UPDATE THE BYTE SIZE
		switch (controlbyte & 0x60)
		{
			case 0x00: bytesize = 8;  break;
			case 0x20: bytesize = 7;  break;
			case 0x40: bytesize = 6;  break;
			case 0x60: bytesize = 5;  break;
		}

		// UPDATE THE NUMBER OF STOP BITS
		if (controlbyte & 0x80)
		{
			if ((bytesize == 8) && (parity == NOPARITY))
				stopbits = ONESTOPBIT;
			else if ((bytesize == 5) && (parity == NOPARITY))
				stopbits = ONE5STOPBITS;
			else
				stopbits = TWOSTOPBITS;
		}
		else
		{
			stopbits = ONESTOPBIT;
		}

		UpdateCommState();
	}

  return controlbyte;
}

//===========================================================================

BYTE __stdcall CommReceive (WORD, BYTE, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return 0;

	BYTE result = 0;
	if (g_vRecvBytes)
	{
		// Don't need critical section in here as CommThread is waiting for ACK

		result = g_RecvBuffer[0];
		--g_vRecvBytes;

		if (g_vbCommIRQ && !g_vRecvBytes)
		{
			// Read last byte, so get CommThread to call WaitCommEvent() again
			OutputDebugString("CommRecv: SetEvent - ACK\n");
			SetEvent(g_hCommEvent[COMMEVT_ACK]);
		}
	}

	return result;
}

//===========================================================================

BYTE __stdcall CommTransmit (WORD, BYTE, BYTE, BYTE value, ULONG)
{
	if (!CheckComm())
		return 0;

	DWORD uBytesWritten;
	WriteFile(commhandle, &value, 1, &uBytesWritten, &o);

	g_bWrittenTx = true;	// Transmit done

	// TO DO:
	// 1) Use CommThread determine when transmit is complete
	// 2) OR do this:
	//if (g_bTxIrqEnabled)
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

BYTE __stdcall CommStatus (WORD, BYTE, BYTE, BYTE, ULONG)
{
	if (!CheckComm())
		return ST_DSR | ST_DCD | ST_TX_EMPTY;

#ifdef SUPPORT_MODEM
	DWORD modemstatus = 0;
	GetCommModemStatus(commhandle,&modemstatus);				// Returns 0x30 = MS_DSR_ON|MS_CTS_ON
#endif

	//

	// TO DO - ST_TX_EMPTY:
	// . IRQs enabled  : set after WaitCommEvent has signaled that TX has completed
	// . IRQs disabled : always set it [Currently done]
	//

	// So that /g_vRecvBytes/ doesn't change midway (from 0 to 1):
	// . bIRQ=false, but uStatus.ST_RX_FULL=1
	EnterCriticalSection(&g_CriticalSection);

	bool bIRQ = false;
	if (g_bTxIrqEnabled && g_bWrittenTx)
	{
		bIRQ = true;
	}
	if (g_bRxIrqEnabled && g_vRecvBytes)
	{
		bIRQ = true;
	}

	g_bWrittenTx = false;	// Read status reg always clears IRQ

	//

	BYTE uStatus = ST_TX_EMPTY 
				| (g_vRecvBytes					? ST_RX_FULL : 0x00)
#ifdef SUPPORT_MODEM
				| ((modemstatus & MS_RLSD_ON)	? 0x00 : ST_DCD)	// Need 0x00 to allow ZLink to start up
				| ((modemstatus & MS_DSR_ON)	? 0x00 : ST_DSR)
#endif
				| (bIRQ							? ST_IRQ : 0x00);

	LeaveCriticalSection(&g_CriticalSection);

	CpuIrqDeassert(IS_SSC);

	return uStatus;
}

//===========================================================================

BYTE __stdcall CommDipSw (WORD, BYTE addr, BYTE, BYTE, ULONG)
{
	// TO DO: determine what values a real SSC returns
	BYTE sw = 0;
	return sw;
}

//===========================================================================

void CommReset ()
{
	CloseComm();

	baudrate    = CBR_19200;
	bytesize    = 8;
	commandbyte = 0x00;
	controlbyte = 0x1F;
	parity      = NOPARITY;
	g_vRecvBytes   = 0;
	stopbits    = ONESTOPBIT;
}

//===========================================================================

void CommDestroy ()
{
	if ((baudrate != CBR_19200) ||
		(bytesize != 8) ||
		(parity   != NOPARITY) ||
		(stopbits != ONESTOPBIT))
	{
		CommReset();
	}

	CloseComm();
}

//===========================================================================

void CommSetSerialPort (HWND window, DWORD newserialport)
{
	if (commhandle == INVALID_HANDLE_VALUE)
	{
		serialport = newserialport;
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

void CommUpdate (DWORD totalcycles)
{
	if (commhandle == INVALID_HANDLE_VALUE)
		return;

	if ((comminactivity += totalcycles) > 1000000)
	{
		static DWORD lastcheck = 0;

		if ((comminactivity > 2000000) || (comminactivity-lastcheck > 99950))
		{
#ifdef SUPPORT_MODEM
			DWORD modemstatus = 0;
			GetCommModemStatus(commhandle,&modemstatus);
			if ((modemstatus & MS_RLSD_ON) || DiskIsSpinning())
				comminactivity = 0;
#else
			if (DiskIsSpinning())
				comminactivity = 0;
#endif
		}

		//if (comminactivity > 2000000)
		//	CloseComm();
	}
}

//===========================================================================

static void CheckCommEvent(DWORD dwEvtMask)
{
	if (dwEvtMask & EV_RXCHAR)
	{
		EnterCriticalSection(&g_CriticalSection);
		ReadFile(commhandle, g_RecvBuffer, 1, (DWORD*)&g_vRecvBytes, &o);
		LeaveCriticalSection(&g_CriticalSection);

		if (g_bRxIrqEnabled && g_vRecvBytes)
		{
			g_vbCommIRQ = true;
			CpuIrqAssert(IS_SSC);
		}
	}
	//else if (dwEvtMask & EV_TXEMPTY)
	//{
	//	if (g_bTxIrqEnabled)
	//	{
	//		g_vbCommIRQ = true;
	//		CpuIrqAssert(IS_SSC);
	//	}
	//}
}

static DWORD WINAPI CommThread(LPVOID lpParameter)
{
	char szDbg[100];
//	BOOL bRes = SetCommMask(commhandle, EV_TXEMPTY | EV_RXCHAR);
	BOOL bRes = SetCommMask(commhandle, EV_RXCHAR);		// Just RX
	if (!bRes)
		return -1;

	//

	const UINT nNumEvents = 2;
#if 1
	HANDLE hCommEvent_Wait[nNumEvents] = {g_hCommEvent[COMMEVT_WAIT], g_hCommEvent[COMMEVT_TERM]};
	HANDLE hCommEvent_Ack[nNumEvents]  = {g_hCommEvent[COMMEVT_ACK],  g_hCommEvent[COMMEVT_TERM]};
#else
	HANDLE hCommEvent_Wait[nNumEvents];
	HANDLE hCommEvent_Ack[nNumEvents];

	hCommEvent_Wait[0] = g_hCommEvent[COMMEVT_WAIT];
	hCommEvent_Wait[1] = g_hCommEvent[COMMEVT_TERM];

	hCommEvent_Ack[0] = g_hCommEvent[COMMEVT_ACK];
	hCommEvent_Ack[1] = g_hCommEvent[COMMEVT_TERM];
#endif

	while(1)
	{
		DWORD dwEvtMask = 0;
		DWORD dwWaitResult;

		bRes = WaitCommEvent(commhandle, &dwEvtMask, &o);	// Will return immediately (probably with ERROR_IO_PENDING)
		_ASSERT(!bRes);
		if (!bRes)
		{
			DWORD dwRet = GetLastError();
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
		CheckCommEvent(dwEvtMask);

		if (g_vbCommIRQ)
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

			g_vbCommIRQ = false;
		}
	}

	return 0;
}

bool CommThInit()
{
	_ASSERT(g_hCommThread == NULL);
	_ASSERT(commhandle);

	if ((g_hCommEvent[0] == NULL) && (g_hCommEvent[1] == NULL) && (g_hCommEvent[2] == NULL))
	{
		// Create an event object for use by WaitCommEvent

		o.hEvent = CreateEvent(
			NULL,   // default security attributes 
			FALSE,  // auto reset event (bManualReset)
			FALSE,  // not signaled (bInitialState)
			NULL    // no name
			);

		// Initialize the rest of the OVERLAPPED structure to zero
		o.Internal = 0;
		o.InternalHigh = 0;
		o.Offset = 0;
		o.OffsetHigh = 0;

		//

		g_hCommEvent[COMMEVT_WAIT] = o.hEvent;
		g_hCommEvent[COMMEVT_ACK] = CreateEvent(NULL,	// lpEventAttributes
										FALSE,	// bManualReset (FALSE = auto-reset)
										FALSE,	// bInitialState (FALSE = non-signaled)
										NULL);	// lpName
		g_hCommEvent[COMMEVT_TERM] = CreateEvent(NULL,	// lpEventAttributes
										FALSE,	// bManualReset (FALSE = auto-reset)
										FALSE,	// bInitialState (FALSE = non-signaled)
										NULL);	// lpName

		if ((g_hCommEvent[0] == NULL) || (g_hCommEvent[1] == NULL) || (g_hCommEvent[2] == NULL))
		{
			if(g_fh) fprintf(g_fh, "Comm: CreateEvent failed\n");
			return false;
		}
	}

	//

	if (g_hCommThread == NULL)
	{
		DWORD dwThreadId;

		g_hCommThread = CreateThread(NULL,			// lpThreadAttributes
									0,				// dwStackSize
									CommThread,
									NULL,			// lpParameter
									0,				// dwCreationFlags : 0 = Run immediately
									&dwThreadId);	// lpThreadId

		InitializeCriticalSection(&g_CriticalSection);
	}

	return true;
}

void CommThUninit()
{
	if (g_hCommThread)
	{
		SetEvent(g_hCommEvent[COMMEVT_TERM]);	// Signal to thread that it should exit

		do
		{
			DWORD dwExitCode;
			if(GetExitCodeThread(g_hCommThread, &dwExitCode))
			{
				if(dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		}
		while(1);

		CloseHandle(g_hCommThread);
		g_hCommThread = NULL;

	  	DeleteCriticalSection(&g_CriticalSection);
	}

	//

	for (UINT i=0; i<COMMEVT_MAX; i++)
	{
		if(g_hCommEvent[i])
		{
			CloseHandle(g_hCommEvent[i]);
			g_hCommEvent[i] = NULL;
		}
	}
}

//===========================================================================

DWORD CommGetSnapshot(SS_IO_Comms* pSS)
{
	pSS->baudrate		= baudrate;
	pSS->bytesize		= bytesize;
	pSS->commandbyte	= commandbyte;
	pSS->comminactivity	= comminactivity;
	pSS->controlbyte	= controlbyte;
	pSS->parity			= parity;
	memcpy(pSS->recvbuffer, g_RecvBuffer, uRecvBufferSize);
	pSS->recvbytes		= g_vRecvBytes;
	pSS->stopbits		= stopbits;
	return 0;
}

DWORD CommSetSnapshot(SS_IO_Comms* pSS)
{
	baudrate		= pSS->baudrate;
	bytesize		= pSS->bytesize;
	commandbyte		= pSS->commandbyte;
	comminactivity	= pSS->comminactivity;
	controlbyte		= pSS->controlbyte;
	parity			= pSS->parity;
	memcpy(g_RecvBuffer, pSS->recvbuffer, uRecvBufferSize);
	g_vRecvBytes	= pSS->recvbytes;
	stopbits		= pSS->stopbits;
	return 0;
}
