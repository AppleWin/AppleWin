#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "../Joystick.h"
class CPropertySheetHelper;
class CConfigNeedingRestart;

class CPageInput : private IPropertySheetPage
{
public:
	CPageInput(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_INPUT),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uCursorControl(kCursorControl_Default),
		m_uCenteringControl(kCenteringControl_Default),
		m_bmAutofire(kAutofire_Default),
		m_bSwapButtons0and1(kSwapButtons0and1_Default)
	{
		CPageInput::ms_this = this;
	}
	virtual ~CPageInput(){}

	static INT_PTR CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetJoystickCursorControl(void){ return m_uCursorControl; }
	void SetJoystickCursorControl(UINT uValue){ m_uCursorControl = uValue; }
	UINT GetJoystickCenteringControl(void){ return m_uCenteringControl; }
	void SetJoystickCenteringControl(UINT uValue){ m_uCenteringControl = uValue; }
	UINT GetAutofire(UINT uButton) { return (m_bmAutofire >> uButton) & 1; }	// Get a specific button
	UINT GetAutofire(void) { return m_bmAutofire; }								// Get all buttons
	void SetAutofire(UINT uValue) { m_bmAutofire = uValue; }					// Set all buttons
	bool GetButtonsSwapState(void){ return m_bSwapButtons0and1; }
	void SetButtonsSwapState(bool value){ m_bSwapButtons0and1 = value; }

	static const UINT kAutofire_Default = 0;
	static const UINT kCenteringControl_Default = JOYSTICK_MODE_CENTERING;
	static const UINT kCursorControl_Default = 1;
	static const bool kSwapButtons0and1_Default = false;

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	void InitJoystickChoices(HWND hWnd, int nJoyNum, int nIdcValue);

	static CPageInput* ms_this;
	static const UINT MaxMenuChoiceLen = 40;

	static const char m_szJoyChoice0[];
	static const char m_szJoyChoice1[];
	static const char m_szJoyChoice2[];
	static const char m_szJoyChoice3[];
	static const char m_szJoyChoice4[];
	static const char m_szJoyChoice5[];
	static const char m_szJoyChoice6[];
	static const char* const m_pszJoy0Choices[J0C_MAX];
	static const char* const m_pszJoy1Choices[J1C_MAX];

	int m_nJoy0ChoiceTranlationTbl[J0C_MAX];
	char m_joystick0choices[J0C_MAX * MaxMenuChoiceLen];
	int m_nJoy1ChoiceTranlationTbl[J1C_MAX];
	char m_joystick1choices[J1C_MAX * MaxMenuChoiceLen];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;

	UINT m_uCursorControl;		// 1 = Allow AppleII to read cursor keys from $C000 (when using keyboard for joystick emu)
	UINT m_uCenteringControl;	// 1 = Centering, 0=Floating (when using keyboard for joystick emu)
	UINT m_bmAutofire;			// bitmask b2:0
	bool m_bSwapButtons0and1;
};
