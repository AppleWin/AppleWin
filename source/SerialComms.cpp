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

/* Description: Enhanced //e & //c serial port emulation
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma  hdrstop

static DWORD  baudrate       = CBR_19200;
static BYTE   bytesize       = 8;
static BYTE   commandbyte    = 0x00;
static HANDLE commhandle     = INVALID_HANDLE_VALUE;
static DWORD  comminactivity = 0;
static BYTE   controlbyte    = 0x1F;
static BYTE   parity         = NOPARITY;
static BYTE   recvbuffer[9];
static DWORD  recvbytes      = 0;
DWORD  serialport     = 0;
static BYTE   stopbits       = ONESTOPBIT;

static void UpdateCommState ();

//===========================================================================
static BOOL CheckComm () {
  comminactivity = 0;
  if ((commhandle == INVALID_HANDLE_VALUE) && serialport) {
    TCHAR portname[8];
    wsprintf(portname,
             TEXT("COM%u"),
             serialport);
    commhandle = CreateFile(portname,
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            (LPSECURITY_ATTRIBUTES)NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);
    if (commhandle != INVALID_HANDLE_VALUE) {
      UpdateCommState();
      COMMTIMEOUTS ct;
      ZeroMemory(&ct,sizeof(COMMTIMEOUTS));
      ct.ReadIntervalTimeout = MAXDWORD;
      SetCommTimeouts(commhandle,&ct);
    }
  }
  return (commhandle != INVALID_HANDLE_VALUE);
}

//===========================================================================
static void CheckReceive () {
  if (recvbytes || (commhandle == INVALID_HANDLE_VALUE))
    return;
  ReadFile(commhandle,recvbuffer,8,&recvbytes,NULL);
}

//===========================================================================
static void CloseComm () {
  if (commhandle != INVALID_HANDLE_VALUE)
    CloseHandle(commhandle);
  commhandle     = INVALID_HANDLE_VALUE;
  comminactivity = 0;
}

//===========================================================================
static void UpdateCommState () {
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
BYTE __stdcall CommCommand (WORD, BYTE, BYTE write, BYTE value, ULONG) {
  if (!CheckComm())
    return 0;
  if (write && (value != commandbyte)) {
    commandbyte = value;

    // UPDATE THE PARITY
    if (commandbyte & 0x20)
      switch (commandbyte & 0xC0) {
        case 0x00 : parity = ODDPARITY;    break;
        case 0x40 : parity = EVENPARITY;   break;
        case 0x80 : parity = MARKPARITY;   break;
        case 0xC0 : parity = SPACEPARITY;  break;
      }
    else
      parity = NOPARITY;

    UpdateCommState();
  }
  return commandbyte;
}

//===========================================================================
BYTE __stdcall CommControl (WORD, BYTE, BYTE write, BYTE value, ULONG) {
  if (!CheckComm())
    return 0;
  if (write && (value != controlbyte)) {
    controlbyte = value;

    // UPDATE THE BAUD RATE
    switch (controlbyte & 0x0F) {
      case 0x00: // fall through
      case 0x01: // fall through
      case 0x02: // fall through
      case 0x03: // fall through
      case 0x04: // fall through
      case 0x05: baudrate = CBR_110;     break;
      case 0x06: baudrate = CBR_300;     break;
      case 0x07: baudrate = CBR_600;     break;
      case 0x08: baudrate = CBR_1200;    break;
      case 0x09: // fall through
      case 0x0A: baudrate = CBR_2400;    break;
      case 0x0B: // fall through
      case 0x0C: baudrate = CBR_4800;    break;
      case 0x0D: // fall through
      case 0x0E: baudrate = CBR_9600;    break;
      case 0x0F: baudrate = CBR_19200;   break;
    }

    // UPDATE THE BYTE SIZE
    switch (controlbyte & 0x60) {
      case 0x00: bytesize = 8;  break;
      case 0x20: bytesize = 7;  break;
      case 0x40: bytesize = 6;  break;
      case 0x60: bytesize = 5;  break;
    }

    // UPDATE THE NUMBER OF STOP BITS
    if (controlbyte & 0x80) {
      if ((bytesize == 8) && (parity == NOPARITY))
        stopbits = ONESTOPBIT;
      else if ((bytesize == 5) && (parity == NOPARITY))
        stopbits = ONE5STOPBITS;
      else
        stopbits = TWOSTOPBITS;
    }
    else
      stopbits = ONESTOPBIT;

    UpdateCommState();
  }
  return controlbyte;
}

//===========================================================================
void CommDestroy () {
  if ((baudrate != CBR_19200) ||
      (bytesize != 8) ||
      (parity   != NOPARITY) ||
      (stopbits != ONESTOPBIT)) {
    CommReset();
    CheckComm();
  }
  CloseComm();
}

//===========================================================================
BYTE __stdcall CommDipSw (WORD, BYTE, BYTE, BYTE, ULONG) {
  // note: determine what values a real SSC returns
  return 0;
}

//===========================================================================
void CommSetSerialPort (HWND window, DWORD newserialport) {
  if (commhandle == INVALID_HANDLE_VALUE)
    serialport = newserialport;
  else
    MessageBox(window,
               TEXT("You cannot change the serial port while it is ")
               TEXT("in use."),
               TEXT("Configuration"),
               MB_ICONEXCLAMATION | MB_SETFOREGROUND);
}

//===========================================================================
void CommUpdate (DWORD totalcycles) {
  if (commhandle == INVALID_HANDLE_VALUE)
    return;
  if ((comminactivity += totalcycles) > 1000000) {
    static DWORD lastcheck = 0;
    if ((comminactivity > 2000000) || (comminactivity-lastcheck > 99950)) {
      DWORD modemstatus = 0;
      GetCommModemStatus(commhandle,&modemstatus);
      if ((modemstatus & MS_RLSD_ON) || DiskIsSpinning())
        comminactivity = 0;
    }
    if (comminactivity > 2000000)
      CloseComm();
  }
}

//===========================================================================
BYTE __stdcall CommReceive (WORD, BYTE, BYTE, BYTE, ULONG) {
  if (!CheckComm())
    return 0;
  if (!recvbytes)
    CheckReceive();
  BYTE result = 0;
  if (recvbytes) {
    result = recvbuffer[0];
    if (--recvbytes)
      MoveMemory(recvbuffer,recvbuffer+1,recvbytes);
  }
  return result;
}

//===========================================================================
void CommReset () {
  CloseComm();
  baudrate    = CBR_19200;
  bytesize    = 8;
  commandbyte = 0x00;
  controlbyte = 0x1F;
  parity      = NOPARITY;
  recvbytes   = 0;
  stopbits    = ONESTOPBIT;
}

//===========================================================================
BYTE __stdcall CommStatus (WORD, BYTE, BYTE, BYTE, ULONG) {
  if (!CheckComm())
    return 0x70;
  if (!recvbytes)
    CheckReceive();
  DWORD modemstatus = 0;
  GetCommModemStatus(commhandle,&modemstatus);
  return 0x10 | (recvbytes                  ? 0x08 : 0x00)
              | ((modemstatus & MS_RLSD_ON) ? 0x00 : 0x20)
              | ((modemstatus & MS_DSR_ON)  ? 0x00 : 0x40);
}

//===========================================================================
BYTE __stdcall CommTransmit (WORD, BYTE, BYTE, BYTE value, ULONG) {
  if (!CheckComm())
    return 0;
  DWORD byteswritten;
  WriteFile(commhandle,&value,1,&byteswritten,NULL);
  return 0;
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
	memcpy(pSS->recvbuffer, recvbuffer, 9);
	pSS->recvbytes		= recvbytes;
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
	memcpy(recvbuffer, pSS->recvbuffer, 9);
	recvbytes		= pSS->recvbytes;
	stopbits		= pSS->stopbits;
	return 0;
}
