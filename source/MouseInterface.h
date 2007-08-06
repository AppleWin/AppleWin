#include "6821.h"
#include "Common.h"

#define WRITE_HANDLER(func)		void func( void* objFrom, void* objTo, int nAddr, BYTE byData )
#define CALLBACK_HANDLER(func)	void func( void* objFrom, void* objTo, LPARAM lParam )

extern class CMouseInterface sg_Mouse;

class CMouseInterface
{
public:
	CMouseInterface();
	virtual ~CMouseInterface();

	void Initialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void Uninitialize(){ m_bActive = false; }
	void SetSlotRom();
	static BYTE __stdcall IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);
	static BYTE __stdcall IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft);

	void SetPosition(int xvalue, int xrange, int yvalue, int yrange);
	void SetButton(eBUTTON Button, eBUTTONSTATE State);
	bool Active() { return m_bActive; }
	void SetVBlank(bool bVBL);

protected:
	void On6821_A(BYTE byData);
	void On6821_B(BYTE byData);
	void OnCommand();
	void OnWrite();
	void OnMouseEvent();
	void Reset();

	friend WRITE_HANDLER( M6821_Listener_A );
	friend WRITE_HANDLER( M6821_Listener_B );
	//friend CALLBACK_HANDLER( MouseHandler );

	void SetPosition(int xvalue, int yvalue);
	void ClampX(int iMinX, int iMaxX);
	void ClampY(int iMinY, int iMaxY);


	C6821	m_6821;

	int		m_nDataLen;
	BYTE	m_byMode;

	BYTE	m_by6821B;
	BYTE	m_by6821A;
	BYTE	m_byBuff[8];			// m_byBuff[0] is mode byte
	int		m_nBuffPos;

	BYTE	m_byState;
	int		m_nX;
	int		m_nY;
	BOOL	m_bBtn0;
	BOOL	m_bBtn1;

	bool	m_bVBL;

	//

	UINT	m_iX;
	UINT	m_iRangeX;
	UINT	m_iMinX;
	UINT	m_iMaxX;
	UINT	m_iY;
	UINT	m_iRangeY;
	UINT	m_iMinY;
	UINT	m_iMaxY;

	BOOL	m_bButtons[2];

	//

	bool	m_bActive;
	LPBYTE	m_pSlotRom;
	UINT	m_uSlot;
};
