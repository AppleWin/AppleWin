// Based on Apple in PC's mousecard.cpp
// - Permission given by Kyle Kim to reuse in AppleWin

#ifdef _DEBUG
#define _DEBUG_SPURIOUS_IRQ

/*
Contiki v1.3:
. I still see 2 cases of abnormal operation. These occur during boot up, during some period of disk access.
. See Contiki's IRQ() in apple2-stdmou.s

** Normal operation **
 
<VBL EVENT>   : h/w asserts IRQ
<6502 jumps to IRQ vector>
. MOUSE_SERVE : h/w deasserts IRQ
. MOUSE_READ
. RTS with C=1
 
<VBL EVENT>   : h/w asserts IRQ
<6502 jumps to IRQ vector>
. MOUSE_SERVE : h/w deasserts IRQ
. MOUSE_READ
. RTS with C=1
 
Etc.
 
** Abnormal operation **
 
<VBL EVENT>                        : h/w asserts IRQ
<6502 jumps to IRQ vector>
. MOUSE_SERVE (for VBL EVENT)      : h/w deasserts IRQ
<VBL EVENT>				           : h/w asserts IRQ
. MOUSE_READ
  - this clears the mouse IRQ status byte in the mouse-card's "h/w"
. RTS with C=1
 
<6502 jumps to IRQ vector>
. MOUSE_SERVE (for MOVEMENT EVENT) : h/w deasserts IRQ
  - but IRQ status byte is 0
. RTS with C=0

*/

#endif

#include "stdafx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
#include "MouseInterface.h"
#include "Frame.h"	// FrameSetCursorPosByMousePos()

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

// Interrupt status byte
                                                //Bit 7 6 5 4 3 2 1 0 
                                                //    | | | | | | | | 
#define STAT_PREV_BUTTON1   (1<<0)              //    | | | | | | | \--- Previously, button 1 was up (0) or down (1)
#define STAT_INT_MOVEMENT   (1<<1)              //    | | | | | | \----- Movement interrupt 
#define STAT_INT_BUTTON     (1<<2)              //    | | | | | \------- Button 0/1 interrupt 
#define STAT_INT_VBL        (1<<3)              //    | | | | \--------- VBL interrupt 
#define STAT_CURR_BUTTON1   (1<<4)              //    | | | \----------- Currently, button 1 is up (0) or down (1) 
#define STAT_MOVEMENT_SINCE_READMOUSE   (1<<5)  //    | | \------------- X/Y moved since last READMOUSE 
#define STAT_PREV_BUTTON0   (1<<6)              //    | \--------------- Previously, button 0 was up (0) or down (1)
#define STAT_CURR_BUTTON0   (1<<7)              //    \----------------- Currently, button 0 is up (0) or down (1) 

#define STAT_INT_ALL		(STAT_INT_VBL | STAT_INT_BUTTON | STAT_INT_MOVEMENT)

// Mode byte
                                                //Bit 7 6 5 4 3 2 1 0 
                                                //    | | | | | | | | 
#define MODE_MOUSE_ON       (1<<0)              //    | | | | | | | \--- Mouse off (0) or on (1) 
#define MODE_INT_MOVEMENT   (1<<1)              //    | | | | | | \----- Interrupt if mouse is moved 
#define MODE_INT_BUTTON	    (1<<2)              //    | | | | | \------- Interrupt if button is pressed
#define MODE_INT_VBL        (1<<3)              //    | | | | \--------- Interrupt on VBL 
#define MODE_RESERVED4      (1<<4)              //    | | | \----------- Reserved 
#define MODE_RESERVED5      (1<<5)              //    | | \------------- Reserved 
#define MODE_RESERVED6      (1<<6)              //    | \--------------- Reserved 
#define MODE_RESERVED7      (1<<7)              //    \----------------- Reserved 

#define MODE_INT_ALL		STAT_INT_ALL

//===========================================================================

void M6821_Listener_B( void* objTo, BYTE byData )
{
	((CMouseInterface*)objTo)->On6821_B( byData );
}

void M6821_Listener_A( void* objTo, BYTE byData )
{
	((CMouseInterface*)objTo)->On6821_A( byData );
}

//===========================================================================

CMouseInterface::CMouseInterface() :
	m_pSlotRom(NULL)
{
	m_6821.SetListenerB( this, M6821_Listener_B );
	m_6821.SetListenerA( this, M6821_Listener_A );

	Uninitialize();
	Reset();
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

	m_bActive = true;
	SetEnabled(true);
	SetSlotRom();	// Pre: m_bActive == true
	RegisterIoHandler(uSlot, &CMouseInterface::IORead, &CMouseInterface::IOWrite, NULL, NULL, this, NULL);
}

void CMouseInterface::Uninitialize()
{
	m_bActive = false;
}

void CMouseInterface::Reset()
{
	m_by6821A = 0;
	m_by6821B = 0x40;		// Set PB6
	m_6821.SetPB(m_by6821B);
	m_bVBL = false;

	//

	m_nX = 0;
	m_nY = 0;

	m_iX = 0;
	m_iMinX = 0;
	m_iMaxX = 1023;

	m_iY = 0;
	m_iMinY = 0;
	m_iMaxY = 1023;

	m_bButtons[0] = m_bButtons[1] = FALSE;

	//

	Clear();
	memset( m_byBuff, 0, sizeof( m_byBuff ) );
	SetSlotRom();
}

void CMouseInterface::SetSlotRom()
{
	if (!m_bActive)
		return;

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
#ifdef _DEBUG_SPURIOUS_IRQ
	static UINT uSpuriousIrqCount = 0;
	char szDbg[200];
	BYTE byOldState = m_byState;
#endif

	switch( m_byBuff[0] & 0xF0 )
	{
	case MOUSE_SET:
		m_nDataLen = 1;
		m_byMode = m_byBuff[0] & 0x0F;
		break;
	case MOUSE_READ:				// Read
		m_nDataLen = 6;
		m_byState &= STAT_MOVEMENT_SINCE_READMOUSE;
		m_nX = m_iX;
		m_nY = m_iY;
		if ( m_bBtn0 )	m_byState |= STAT_PREV_BUTTON0;	// Previous Button 0
		if ( m_bBtn1 )	m_byState |= STAT_PREV_BUTTON1;	// Previous Button 1
		m_bBtn0 = m_bButtons[0];
		m_bBtn1 = m_bButtons[1];
		if ( m_bBtn0 )	m_byState |= STAT_CURR_BUTTON0;	// Current Button 0
		if ( m_bBtn1 )	m_byState |= STAT_CURR_BUTTON1;	// Current Button 1
		m_byBuff[1] = m_nX & 0xFF;
		m_byBuff[2] = ( m_nX >> 8 ) & 0xFF;
		m_byBuff[3] = m_nY & 0xFF;
		m_byBuff[4] = ( m_nY >> 8 ) & 0xFF;
		m_byBuff[5] = m_byState;					// button 0/1 interrupt status
		m_byState &= ~STAT_MOVEMENT_SINCE_READMOUSE;
#ifdef _DEBUG_SPURIOUS_IRQ
		sprintf(szDbg, "[MOUSE_READ] Old=%02X New=%02X\n", byOldState, m_byState); OutputDebugString(szDbg);
#endif
		break;
	case MOUSE_SERV:
		m_nDataLen = 2;
		m_byBuff[1] = m_byState & ~STAT_MOVEMENT_SINCE_READMOUSE;			// reason of interrupt
#ifdef _DEBUG_SPURIOUS_IRQ
		if ((m_byMode & MODE_INT_ALL) && (m_byBuff[1] & MODE_INT_ALL) == 0)
		{
			uSpuriousIrqCount++;
			sprintf(szDbg, "[MOUSE_SERV] 0x%04X Buff[1]=0x%02X, ***\n", uSpuriousIrqCount, m_byBuff[1]); OutputDebugString(szDbg);
		}
		else
		{
			sprintf(szDbg, "[MOUSE_SERV] ------ Buff[1]=0x%02X\n", m_byBuff[1]); OutputDebugString(szDbg);
		}
#endif
		CpuIrqDeassert(IS_MOUSE);
		break;
	case MOUSE_CLEAR:
		Clear();					// [TC] NB. Don't reset clamp values (eg. Fantavision)
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
		SetPositionAbs( 0, 0 );
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
		// Blazing Paddles:
		// . MOUSE_CLAMP(Y, 0xFFEC, 0x00D3)
		// . MOUSE_CLAMP(X, 0xFFEC, 0x012B)
		nMin = ( m_byBuff[3] << 8 ) | m_byBuff[1];
		nMax = ( m_byBuff[4] << 8 ) | m_byBuff[2];
		if ( m_byBuff[0] & 1 )	// Clamp Y
			SetClampY( nMin, nMax );
		else					// Clamp X
			SetClampX( nMin, nMax );
		break;
	case MOUSE_POS:
		m_nX = ( m_byBuff[2] << 8 ) | m_byBuff[1];
		m_nY = ( m_byBuff[4] << 8 ) | m_byBuff[3];
		SetPositionAbs( m_nX, m_nY );
		break;
	case MOUSE_INIT:
		m_nX = 0;
		m_nY = 0;
		SetClampX( 0, 1023 );
		SetClampY( 0, 1023 );
		SetPositionAbs( 0, 0 );
		break;
	}
}

void CMouseInterface::OnMouseEvent(bool bEventVBL)
{
	int byState = 0;

	if ( !( m_byMode & MODE_MOUSE_ON ) )		// Mouse Off
		return;

	BOOL bBtn0 = m_bButtons[0];
	BOOL bBtn1 = m_bButtons[1];
	if ( m_nX != m_iX || m_nY != m_iY )
		byState |= STAT_INT_MOVEMENT|STAT_MOVEMENT_SINCE_READMOUSE;	// X/Y moved since last READMOUSE | Movement interrupt
	if ( m_bBtn0 != bBtn0 || m_bBtn1 != bBtn1 )
		byState |= STAT_INT_BUTTON;		// Button 0/1 interrupt
	if ( bEventVBL )
		byState |= STAT_INT_VBL;

	//byState &= m_byMode & 0x2E;
	byState &= ((m_byMode & MODE_INT_ALL) | STAT_MOVEMENT_SINCE_READMOUSE);	// [TC] Keep "X/Y moved since last READMOUSE" for next MOUSE_READ (Contiki v1.3 uses this)

	if ( byState & STAT_INT_ALL )
	{
		m_byState |= byState;
		CpuIrqAssert(IS_MOUSE);
#ifdef _DEBUG_SPURIOUS_IRQ
		char szDbg[200]; sprintf(szDbg, "[MOUSE EVNT] 0x%02X Mode=0x%02X\n", m_byState, m_byMode); OutputDebugString(szDbg);
#endif
	}
}

void CMouseInterface::SetVBlank(bool bVBL)
{
	if ( m_bVBL != bVBL )
	{
		m_bVBL = bVBL;
		if ( m_bVBL )	// Rising edge
			OnMouseEvent(true);
	}
}

void CMouseInterface::Clear()
{
	m_nBuffPos = 0;
	m_nDataLen = 1;

	m_byMode = 0;
	m_byState = 0;
	m_nX = 0;
	m_nY = 0;
	m_bBtn0 = 0;
	m_bBtn1 = 0;
	SetPositionAbs( 0, 0 );

//	CpuIrqDeassert(IS_MOUSE);
}

//===========================================================================

int CMouseInterface::ClampX()
{
	if ( m_iX > m_iMaxX )
	{
		m_iX = m_iMaxX;
		return 1;
	}
	else if ( m_iX < m_iMinX )
	{
		m_iX = m_iMinX;
		return -1;
	}

	return 0;
}

int CMouseInterface::ClampY()
{
	if ( m_iY > m_iMaxY )
	{
		m_iY = m_iMaxY;
		return 1;
	}
	else if ( m_iY < m_iMinY )
	{
		m_iY = m_iMinY;
		return -1;
	}

	return 0;
}

void CMouseInterface::SetClampX(int iMinX, int iMaxX)
{
	if ( (UINT)iMinX > 0xFFFF || (UINT)iMaxX > 0xFFFF )
	{
		_ASSERT(0);
		return;
	}
	if ( iMinX > iMaxX )
	{
		// For Blazing Paddles
		int iNewMaxX = (iMinX + iMaxX) & 0xFFFF;
		iMinX = 0;
		iMaxX = iNewMaxX;
	}
	m_iMaxX = iMaxX;
	m_iMinX = iMinX;
	ClampX();
}

void CMouseInterface::SetClampY(int iMinY, int iMaxY)
{
	if ( (UINT)iMinY > 0xFFFF || (UINT)iMaxY > 0xFFFF )
	{
		_ASSERT(0);
		return;
	}
	if ( iMinY > iMaxY )
	{
		// For Blazing Paddles
		int iNewMaxY = (iMinY + iMaxY) & 0xFFFF;
		iMinY = 0;
		iMaxY = iNewMaxY;
	}
	m_iMaxY = iMaxY;
	m_iMinY = iMinY;
	ClampY();
}

void CMouseInterface::SetPositionAbs(int x, int y)
{
	m_iX = x;
	m_iY = y;
	FrameSetCursorPosByMousePos();
}

void CMouseInterface::SetPositionRel(long dX, long dY, int* pOutOfBoundsX, int* pOutOfBoundsY)
{
	m_iX += dX;
	*pOutOfBoundsX = ClampX();

	m_iY += dY;
	*pOutOfBoundsY = ClampY();

	OnMouseEvent();
}

void CMouseInterface::SetButton(eBUTTON Button, eBUTTONSTATE State)
{
	m_bButtons[Button]= (State == BUTTON_DOWN) ? TRUE : FALSE;
	OnMouseEvent();
}

//=============================================================================
// DirectInput interface
//=============================================================================

//#define STRICT
#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>

namespace DIMouse
{

	static LPDIRECTINPUT8       g_pDI    = NULL;         
	static LPDIRECTINPUTDEVICE8 g_pMouse = NULL;     
	#define SAMPLE_BUFFER_SIZE  16      // arbitrary number of buffer elements

	static UINT_PTR g_TimerIDEvent = 0;

	//-----------------------------------------------------------------------------
	// Name: OnCreateDevice()
	// Desc: Sets up the mouse device using the flags from the dialog.
	//-----------------------------------------------------------------------------
	HRESULT DirectInputInit( HWND hDlg )
	{
		HRESULT hr;
		BOOL    bExclusive;
		BOOL    bForeground;
		BOOL    bImmediate;
		DWORD   dwCoopFlags;

		DirectInputUninit(hDlg);

		// Determine where the buffer would like to be allocated 
		bExclusive         = FALSE;
		bForeground        = FALSE;	// Otherwise get DIERR_OTHERAPPHASPRIO (== E_ACCESSDENIED) on Acquire()
		bImmediate         = TRUE;

		if( bExclusive )
			dwCoopFlags = DISCL_EXCLUSIVE;
		else
			dwCoopFlags = DISCL_NONEXCLUSIVE;

		if( bForeground )
			dwCoopFlags |= DISCL_FOREGROUND;
		else
			dwCoopFlags |= DISCL_BACKGROUND;

		// Create a DInput object
		if( FAILED( hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, 
												IID_IDirectInput8, (VOID**)&g_pDI, NULL ) ) )
			return hr;

		// Obtain an interface to the system mouse device.
		if( FAILED( hr = g_pDI->CreateDevice( GUID_SysMouse, &g_pMouse, NULL ) ) )
			return hr;

		// Set the data format to "mouse format" - a predefined data format 
		//
		// A data format specifies which controls on a device we
		// are interested in, and how they should be reported.
		//
		// This tells DirectInput that we will be passing a
		// DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
		if( FAILED( hr = g_pMouse->SetDataFormat( &c_dfDIMouse2 ) ) )
			return hr;

		// Set the cooperativity level to let DirectInput know how
		// this device should interact with the system and with other
		// DirectInput applications.
		hr = g_pMouse->SetCooperativeLevel( hDlg, dwCoopFlags );
		if( hr == DIERR_UNSUPPORTED && !bForeground && bExclusive )
		{
			DirectInputUninit(hDlg);
			//MessageBox( hDlg, _T("SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n")
			//                  _T("For security reasons, background exclusive mouse\n")
			//                  _T("access is not allowed."), 
			//                  _T("Mouse"), MB_OK );
			return S_OK;
		}

		if( FAILED(hr) )
			return hr;

		if( !bImmediate )
		{
			// IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
			//
			// DirectInput uses unbuffered I/O (buffer size = 0) by default.
			// If you want to read buffered data, you need to set a nonzero
			// buffer size.
			//
			// Set the buffer size to SAMPLE_BUFFER_SIZE (defined above) elements.
			//
			// The buffer size is a DWORD property associated with the device.
			DIPROPDWORD dipdw;
			dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
			dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			dipdw.diph.dwObj        = 0;
			dipdw.diph.dwHow        = DIPH_DEVICE;
			dipdw.dwData            = SAMPLE_BUFFER_SIZE; // Arbitary buffer size

			if( FAILED( hr = g_pMouse->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph ) ) )
				return hr;
		}

		// Acquire the newly created device
		if (FAILED(hr = g_pMouse->Acquire()))
			return hr;

		// Setup timer to read mouse position
		if (g_TimerIDEvent = SetTimer(hDlg, IDEVENT_TIMER_MOUSE, 8, NULL) == 0)	// 120Hz timer
			return -1;

		return S_OK;
	}

	//-----------------------------------------------------------------------------
	// Name: FreeDirectInput()
	// Desc: Initialize the DirectInput variables.
	//-----------------------------------------------------------------------------
	void DirectInputUninit( HWND hDlg )
	{
		// Unacquire the device one last time just in case 
		// the app tried to exit while the device is still acquired.
		if( g_pMouse ) 
			g_pMouse->Unacquire();

		// Release any DirectInput objects.
		SAFE_RELEASE( g_pMouse );
		SAFE_RELEASE( g_pDI );

		if (g_TimerIDEvent)
			KillTimer(hDlg, g_TimerIDEvent);
	}

	//-----------------------------------------------------------------------------
	// Name: ReadImmediateData()
	// Desc: Read the input device's state when in immediate mode and display it.
	//-----------------------------------------------------------------------------
	HRESULT ReadImmediateData( long* pX, long* pY )
	{
		HRESULT       hr;
		DIMOUSESTATE2 dims2;      // DirectInput mouse state structure

		if (pX) *pX = 0;
		if (pY) *pY = 0;

		if( NULL == g_pMouse )
			return S_OK;

		// Get the input's device state, and put the state in dims
		ZeroMemory( &dims2, sizeof(dims2) );
		hr = g_pMouse->GetDeviceState( sizeof(DIMOUSESTATE2), &dims2 );
		if( FAILED(hr) ) 
		{
			// DirectInput may be telling us that the input stream has been
			// interrupted.  We aren't tracking any state between polls, so
			// we don't have any special reset that needs to be done.
			// We just re-acquire and try again.

			// If input is lost then acquire and keep trying 
			hr = g_pMouse->Acquire();
			while( hr == DIERR_INPUTLOST ) 
				hr = g_pMouse->Acquire();

			// Update the dialog text 
			if( hr == DIERR_OTHERAPPHASPRIO || 
				hr == DIERR_NOTACQUIRED ) 
			{
				//SetDlgItemText( hDlg, IDC_DATA, TEXT("Unacquired") );
			}

			// hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
			// may occur when the app is minimized or in the process of 
			// switching, so just try again later 
			return S_OK; 
		}

		if (pX) *pX = dims2.lX;
		if (pY) *pY = dims2.lY;

		return S_OK;
	}

};	// namespace DIMouse