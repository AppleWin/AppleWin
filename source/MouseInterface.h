#include "6821.h"
#include "Common.h"

extern class CMouseInterface sg_Mouse;

class CMouseInterface
{
public:
	CMouseInterface();
	virtual ~CMouseInterface();

	void Initialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void Uninitialize();
	void Reset();
	static BYTE __stdcall IORead(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

	void SetPositionRel(long dx, long dy, int* pOutOfBoundsX, int* pOutOfBoundsY);
	void SetButton(eBUTTON Button, eBUTTONSTATE State);
	bool IsActive() { return m_bActive; }
	bool IsEnabled() { return m_bEnabled; }	// NB. m_bEnabled == true implies that m_bActive == true
	bool IsActiveAndEnabled() { return IsActive() && IsEnabled(); }	// todo: just use IsEnabled()
	void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }
	void SetVBlank(bool bVBL);
	void GetXY(int& iX, int& iMinX, int& iMaxX, int& iY, int& iMinY, int& iMaxY)
	{
		iX    = m_iX;
		iMinX = m_iMinX;
		iMaxX = m_iMaxX;
		iY    = m_iY;
		iMinY = m_iMinY;
		iMaxY = m_iMaxY;
	}
	void SetCursorPos(int iX, int iY)
	{
		m_iX = iX;
		m_iY = iY;
	}

	std::string GetSnapshotCardName(void);
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

protected:
	void SetSlotRom();
	void On6821_A(BYTE byData);
	void On6821_B(BYTE byData);
	void OnCommand();
	void OnWrite();
	void OnMouseEvent(bool bEventVBL=false);
	void Clear();

	friend void M6821_Listener_A( void* objTo, BYTE byData );
	friend void M6821_Listener_B( void* objTo, BYTE byData );

	void SetPositionAbs(int x, int y);
	int ClampX();
	int ClampY();
	void SetClampX(int iMinX, int iMaxX);
	void SetClampY(int iMinY, int iMaxY);

	void SaveSnapshotMC6821(class YamlSaveHelper& yamlSaveHelper, std::string key);
	void LoadSnapshotMC6821(class YamlLoadHelper& yamlLoadHelper, std::string key);

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
	bool	m_bBtn0;
	bool	m_bBtn1;

	bool	m_bVBL;

	//

	int		m_iX;
	int		m_iMinX;
	int		m_iMaxX;
	int		m_iY;
	int		m_iMinY;
	int		m_iMaxY;

	bool	m_bButtons[2];

	//

	// todo: remove m_bActive:
	// - instantiate CMouseInterface object when active (and delete when inactive)
	bool	m_bActive;		// Mouse h/w is active within the Apple][ VM
	bool	m_bEnabled;		// Windows' mouse events get passed to Apple]['s mouse h/w (m_bEnabled == true implies that m_bActive == true)
	LPBYTE	m_pSlotRom;
	UINT	m_uSlot;
};

namespace DIMouse
{
	HRESULT DirectInputInit( HWND hDlg );
	void DirectInputUninit( HWND hDlg );
	HRESULT ReadImmediateData( long* pX=NULL, long* pY=NULL );
};