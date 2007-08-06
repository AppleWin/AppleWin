// Based on Apple in PC's mousecard.cpp
// - Permission given by Kyle Kim to reuse in AppleWin

#include "stdafx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
#include "MouseInterface.h"

// Sets mouse mode
#define MOUSE_SET		0x00
// Reads mouse position
#define MOUSE_READ		0x10
// Services mouse interrupt
#define MOUSE_SERV		0x20
// Clears mouse positions to 0 (for delta mode)
#define MOUSE_CLEAR		0x30
// Sets mouse position to a user-defined pos
#define MOUSE_POS		0x40
// Resets mouse clamps to default values
// Sets mouse position to 0,0
#define MOUSE_INIT		0x50
// Sets mouse bounds in a window
#define MOUSE_CLAMP		0x60
// Sets mouse to upper-left corner of clamp win
#define MOUSE_HOME		0x70

// Set VBL Timing : 0x90 is 60Hz, 0x91 is 50Hz
#define MOUSE_TIME		0x90

#define BIT0		0x01
#define BIT1		0x02
#define BIT2		0x04
#define BIT3		0x08
#define BIT4		0x10
#define BIT5		0x20
#define BIT6		0x40
#define BIT7		0x80

WRITE_HANDLER( M6821_Listener_B )
{
	((CMouseInterface*)objTo)->On6821_B( byData );
}

WRITE_HANDLER( M6821_Listener_A )
{
	((CMouseInterface*)objTo)->On6821_A( byData );
}

//CALLBACK_HANDLER( MouseHandler )
//{
//	((CMouseInterface*)objTo)->OnMouseEvent();
//}

//===========================================================================

CMouseInterface::CMouseInterface() :
	m_pSlotRom(NULL)
{
	m_6821.SetListenerB( this, M6821_Listener_B );
	m_6821.SetListenerA( this, M6821_Listener_A );
//	g_cDIMouse.SetMouseListener( this, MouseHandler );
	m_by6821A = 0;
	m_by6821B = 0x40;		// Set PB6
	m_6821.SetPB(m_by6821B);
	m_bVBL = FALSE;

	//

	m_iX = 0;
	m_iMinX = 0;
	m_iMaxX = 1023;
	m_iRangeX = 0;

	m_iY = 0;
	m_iMinY = 0;
	m_iMaxY = 1023;
	m_iRangeY = 0;

	m_bButtons[0] = m_bButtons[1] = FALSE;

	//

	Reset();
	memset( m_byBuff, 0, sizeof( m_byBuff ) );
	m_bActive = false;
}

CMouseInterface::~CMouseInterface()
{
	delete [] m_pSlotRom;
}

//===========================================================================

void CMouseInterface::Initialize(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	const UINT FW_SIZE = 2*1024;

	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_MOUSEINTERFACE_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != FW_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	m_uSlot = uSlot;

	if (m_pSlotRom == NULL)
	{
		m_pSlotRom = new BYTE [FW_SIZE];

		if (m_pSlotRom)
			memcpy(m_pSlotRom, pData, FW_SIZE);
	}

	//

	SetSlotRom();
	RegisterIoHandler(uSlot, &CMouseInterface::IORead, &CMouseInterface::IOWrite, NULL, NULL, this, NULL);
	m_bActive = true;
}

void CMouseInterface::SetSlotRom()
{
	LPBYTE pCxRomPeripheral = MemGetCxRomPeripheral();
	if (pCxRomPeripheral == NULL)
		return;

	UINT uOffset = (m_by6821B << 7) & 0x0700;
	memcpy(pCxRomPeripheral+m_uSlot*256, m_pSlotRom+uOffset, 256);
	if (mem)
		memcpy(mem+0xC000+m_uSlot*256, m_pSlotRom+uOffset, 256);
}

//===========================================================================

BYTE __stdcall CMouseInterface::IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CMouseInterface* pMouseIF = (CMouseInterface*) MemGetSlotParameters(uSlot);

	BYTE byRS;
	byRS = uAddr & 3;
	return pMouseIF->m_6821.Read( byRS );
}

BYTE __stdcall CMouseInterface::IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	CMouseInterface* pMouseIF = (CMouseInterface*) MemGetSlotParameters(uSlot);

	BYTE byRS;
	byRS = uAddr & 3;
	pMouseIF->m_6821.Write( byRS, uValue );

	return 0;
}

//===========================================================================

void CMouseInterface::On6821_A(BYTE byData)
{
	m_by6821A = byData;
}

void CMouseInterface::On6821_B(BYTE byData)
{
	BYTE byDiff = ( m_by6821B ^ byData ) & 0x3E;

	if ( byDiff )
	{
		m_by6821B &= ~0x3E;
		m_by6821B |= byData & 0x3E;
		if ( byDiff & BIT5 )			// Write to 0285 chip
		{
			if ( byData & BIT5 )
				m_by6821B |= BIT7;		// OK, I'm ready to read from MC6821
			else						// Clock Activate for read
			{
				m_byBuff[m_nBuffPos++] = m_by6821A;
				if ( m_nBuffPos == 1 )
					OnCommand();
				if ( m_nBuffPos == m_nDataLen || m_nBuffPos > 7 )
				{
					OnWrite();			// Have written all, Commit the command.
					m_nBuffPos = 0;
				}
				m_by6821B &= ~BIT7;		// for next reading
				m_6821.SetPB( m_by6821B );
			}
			
		}
		if ( byDiff & BIT4 )		// Read from 0285 chip ?
		{
			if ( byData & BIT4 )
				m_by6821B &= ~BIT6;		// OK, I'll prepare next value
			else						// Clock Activate for write
			{
				if ( m_nBuffPos )		// if m_nBuffPos is 0, something goes wrong!
					m_nBuffPos++;
				if ( m_nBuffPos == m_nDataLen || m_nBuffPos > 7 )
					m_nBuffPos = 0;			// Have read all, ready for next command.
				else
					m_6821.SetPA( m_byBuff[m_nBuffPos] );
				m_by6821B |= BIT6;		// for next writing
			}
		}
		m_6821.SetPB( m_by6821B );

		//

		SetSlotRom();	// Update Cn00 ROM page
	}
}


void CMouseInterface::OnCommand()
{
	//char szDbg[256];
	switch( m_byBuff[0] & 0xF0 )
	{
	case MOUSE_SET:
		m_nDataLen = 1;
		m_byMode = m_byBuff[0] & 0x0F;
		break;
	case MOUSE_READ:				// Read
		m_nDataLen = 6;
		m_byState &= 0x20;
		m_nX = m_iX;
		m_nY = m_iY;
		if ( m_bBtn0 )	m_byState |= 0x40;			// Previous Button 0
		if ( m_bBtn1 )	m_byState |= 0x01;			// Previous Button 1
		m_bBtn0 = m_bButtons[0];
		m_bBtn1 = m_bButtons[1];
		if ( m_bBtn0 )	m_byState |= 0x80;			// Current Button 0
		if ( m_bBtn1 )	m_byState |= 0x10;			// Current Button 1
		m_byBuff[1] = m_nX & 0xFF;
		m_byBuff[2] = ( m_nX >> 8 ) & 0xFF;
		m_byBuff[3] = m_nY & 0xFF;
		m_byBuff[4] = ( m_nY >> 8 ) & 0xFF;
		m_byBuff[5] = m_byState;			// button 0/1 interrupt status
		m_byState &= ~0x20;
		//sprintf(szDbg, "[MOUSE-READ] IRQ=0x%02X X=(0x%02X-0x%02X) Y=(0x%02X-0x%02X) \n", m_byBuff[5], m_byBuff[1], m_byBuff[2], m_byBuff[3], m_byBuff[4]); OutputDebugString(szDbg);
		break;
	case MOUSE_SERV:
		m_nDataLen = 2;
		m_byBuff[1] = m_byState & ~0x20;			// reason of interrupt
		CpuIrqDeassert(IS_MOUSE);
		//sprintf(szDbg, "[MOUSE-SERV] IRQ=0x%02X\n", m_byBuff[1]); OutputDebugString(szDbg);
		break;
	case MOUSE_CLEAR:
		Reset();
		m_nDataLen = 1;
		break;
	case MOUSE_POS:
		m_nDataLen = 5;
		break;
	case MOUSE_INIT:
		m_nDataLen = 3;
		m_byBuff[1] = 0xFF;			// I don't know what it is
		break;
	case MOUSE_CLAMP:
		m_nDataLen = 5;
		break;
	case MOUSE_HOME:
		m_nDataLen = 1;
		SetPosition( 0, 0 );
		break;
	case MOUSE_TIME:		// 0x90
		switch( m_byBuff[0] & 0x0C )
		{
		case 0x00: m_nDataLen = 1; break;	// write cmd ( #$90 is DATATIME 60Hz, #$91 is 50Hz )
		case 0x04: m_nDataLen = 3; break;	// write cmd, $0478, $04F8
		case 0x08: m_nDataLen = 2; break;	// write cmd, $0578
		case 0x0C: m_nDataLen = 4; break;	// write cmd, $0478, $04F8, $0578
		}
		break;
	case 0xA0:
		m_nDataLen = 2;
		break;
	case 0xB0:
	case 0xC0:
		m_nDataLen = 1;
		break;
	default:
		m_nDataLen = 1;
		//TRACE( "CMD : UNKNOWN CMD : #$%02X\n", m_byBuff[0] );
		//_ASSERT(0);
		break;
	}
	m_6821.SetPA( m_byBuff[1] );
}

void CMouseInterface::OnWrite()
{
	int nMin, nMax;
	switch( m_byBuff[0] & 0xF0 )
	{
	case MOUSE_CLAMP:
		nMin = ( m_byBuff[3] << 8 ) | m_byBuff[1];
		nMax = ( m_byBuff[4] << 8 ) | m_byBuff[2];
		if ( m_byBuff[0] & 1 )	// Clamp Y
			ClampY( nMin, nMax );
		else					// Clamp X
			ClampX( nMin, nMax );
		break;
	case MOUSE_POS:
		m_nX = ( m_byBuff[2] << 8 ) | m_byBuff[1];
		m_nY = ( m_byBuff[4] << 8 ) | m_byBuff[3];
		SetPosition( m_nX, m_nY );
		break;
	case MOUSE_INIT:
		m_nX = 0;
		m_nY = 0;
		ClampX( 0, 1023 );
		ClampY( 0, 1023 );
		SetPosition( 0, 0 );
		break;
	}
}

void CMouseInterface::OnMouseEvent()
{
	int byState = 0;

	if ( !( m_byMode & 1 ) )		// Mouse Off
		return;

	BOOL bBtn0 = m_bButtons[0];
	BOOL bBtn1 = m_bButtons[1];
	if ( m_nX != m_iX || m_nY != m_iY )
		byState |= 0x22;				// X/Y moved since last READMOUSE | Movement interrupt
	if ( m_bBtn0 != bBtn0 || m_bBtn1 != bBtn1 )
		byState |= 0x04;				// Button 0/1 interrupt
	if ( m_bVBL )
		byState |= 0x08;
	byState &= m_byMode & 0x2E;

	if ( byState & 0x0E )
	{
		//char szDbg[256];
		//sprintf(szDbg, "[MOUSE] 0x%02X, %04d(%04d), %04d(%04d), %d\n", byState, m_iX, m_nX, m_iY, m_nY, bBtn0); OutputDebugString(szDbg);
		m_byState |= byState;
		CpuIrqAssert(IS_MOUSE);
	}
}

void CMouseInterface::SetVBlank(bool bVBL)
{
	if ( m_bVBL != bVBL )
	{
		m_bVBL = bVBL;
		if ( m_bVBL )	// Rising edge
			OnMouseEvent();
	}
}

void CMouseInterface::Reset()
{
	m_nBuffPos = 0;
	m_nDataLen = 1;

	m_byMode = 0;
	m_byState = 0;
	m_nX = 0;
	m_nY = 0;
	m_bBtn0 = 0;
	m_bBtn1 = 0;
	ClampX( 0, 1023 );
	ClampY( 0, 1023 );
	SetPosition( 0, 0 );

//	CpuIrqDeassert(IS_MOUSE);
}

//===========================================================================

void CMouseInterface::ClampX(int iMinX, int iMaxX)
{
	if ( iMinX < 0 || iMinX > iMaxX )
		return;
	m_iMaxX = iMaxX;
	m_iMinX = iMinX;
	if ( m_iX > m_iMaxX ) m_iX = m_iMaxX; else if ( m_iX < m_iMinX ) m_iX = m_iMinX;
}

void CMouseInterface::ClampY(int iMinY, int iMaxY)
{
	if ( iMinY < 0 || iMinY > iMaxY )
		return;
	m_iMaxY = iMaxY;
	m_iMinY = iMinY;
	if ( m_iY > m_iMaxY ) m_iY = m_iMaxY; else if ( m_iY < m_iMinX ) m_iY = m_iMinY;
}

void CMouseInterface::SetPosition(int xvalue, int yvalue)
{
	if ((m_iRangeX == 0) || (m_iRangeY == 0))
	{
		m_iX = m_iMinX;
		m_iY = m_iMinY;
		return;
	}

	m_iX = (UINT) ((xvalue*m_iMaxX) / m_iRangeX);
	m_iY = (UINT) ((yvalue*m_iMaxY) / m_iRangeY);
}

void CMouseInterface::SetPosition(int xvalue, int xrange, int yvalue, int yrange)
{
	m_iRangeX = (UINT) xrange;
	m_iRangeY = (UINT) yrange;

	SetPosition(xvalue, yvalue);
	OnMouseEvent();
}

void CMouseInterface::SetButton(eBUTTON Button, eBUTTONSTATE State)
{
	m_bButtons[Button]= (State == BUTTON_DOWN) ? TRUE : FALSE;
	OnMouseEvent();
}
