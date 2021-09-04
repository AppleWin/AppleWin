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
		m_uScrollLockToggle(0),
		m_uCursorControl(1),
		m_uCenteringControl(JOYSTICK_MODE_CENTERING),
		m_bmAutofire(0),
		m_bSwapButtons0and1(false),
		m_uMouseShowCrosshair(0),
		m_uMouseRestrictToWindow(0),
		m_CPMChoice(CPM_UNPLUGGED),
		m_FourPlayChoice(FOURPLAY_UNPLUGGED),
		m_SNESMAXChoice(SNESMAX_UNPLUGGED)
	{
		CPageInput::ms_this = this;
	}
	virtual ~CPageInput(){}

	static INT_PTR CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetScrollLockToggle(void){ return m_uScrollLockToggle; }
	void SetScrollLockToggle(UINT uValue){ m_uScrollLockToggle = uValue; }
	UINT GetJoystickCursorControl(void){ return m_uCursorControl; }
	void SetJoystickCursorControl(UINT uValue){ m_uCursorControl = uValue; }
	UINT GetJoystickCenteringControl(void){ return m_uCenteringControl; }
	void SetJoystickCenteringControl(UINT uValue){ m_uCenteringControl = uValue; }
	UINT GetAutofire(UINT uButton) { return (m_bmAutofire >> uButton) & 1; }	// Get a specific button
	void SetAutofire(UINT uValue) { m_bmAutofire = uValue; }					// Set all buttons
	bool GetButtonsSwapState(void){ return m_bSwapButtons0and1; }
	void SetButtonsSwapState(bool value){ m_bSwapButtons0and1 = value; }
	UINT GetMouseShowCrosshair(void){ return m_uMouseShowCrosshair; }
	void SetMouseShowCrosshair(UINT uValue){ m_uMouseShowCrosshair = uValue; }
	UINT GetMouseRestrictToWindow(void){ return m_uMouseRestrictToWindow; }
	void SetMouseRestrictToWindow(UINT uValue){ m_uMouseRestrictToWindow = uValue; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	void InitJoystickChoices(HWND hWnd, int nJoyNum, int nIdcValue);
	void InitSlotOptions(HWND hWnd);
	void InitCPMChoices(HWND hWnd);
	void InitFourPlayChoices(HWND hWnd);
	void InitSNESMAXChoices(HWND hWnd);

	static CPageInput* ms_this;
	static const UINT MaxMenuChoiceLen = 40;

	static const TCHAR m_szJoyChoice0[];
	static const TCHAR m_szJoyChoice1[];
	static const TCHAR m_szJoyChoice2[];
	static const TCHAR m_szJoyChoice3[];
	static const TCHAR m_szJoyChoice4[];
	static const TCHAR m_szJoyChoice5[];
	static const TCHAR m_szJoyChoice6[];
	static const TCHAR* const m_pszJoy0Choices[J0C_MAX];
	static const TCHAR* const m_pszJoy1Choices[J1C_MAX];

	static const TCHAR m_szCPMSlotChoice_Slot4[];
	static const TCHAR m_szCPMSlotChoice_Slot5[];
	static const TCHAR m_szCPMSlotChoice_Unplugged[];
	static const TCHAR m_szCPMSlotChoice_Unavailable[];

	static const TCHAR m_szFourPlaySlotChoice_Slot3[];
	static const TCHAR m_szFourPlaySlotChoice_Slot4[];
	static const TCHAR m_szFourPlaySlotChoice_Slot5[];
	static const TCHAR m_szFourPlaySlotChoice_Unplugged[];
	static const TCHAR m_szFourPlaySlotChoice_Unavailable[];

	static const TCHAR m_szSNESMAXSlotChoice_Slot3[];
	static const TCHAR m_szSNESMAXSlotChoice_Slot4[];
	static const TCHAR m_szSNESMAXSlotChoice_Slot5[];
	static const TCHAR m_szSNESMAXSlotChoice_Unplugged[];
	static const TCHAR m_szSNESMAXSlotChoice_Unavailable[];

	int m_nJoy0ChoiceTranlationTbl[J0C_MAX];
	TCHAR m_joystick0choices[J0C_MAX * MaxMenuChoiceLen];
	int m_nJoy1ChoiceTranlationTbl[J1C_MAX];
	TCHAR m_joystick1choices[J1C_MAX * MaxMenuChoiceLen];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;

	UINT m_uScrollLockToggle;
	UINT m_uCursorControl;		// 1 = Allow AppleII to read cursor keys from $C000 (when using keyboard for joystick emu)
	UINT m_uCenteringControl;	// 1 = Centering, 0=Floating (when using keyboard for joystick emu)
	UINT m_bmAutofire;			// bitmask b2:0
	bool m_bSwapButtons0and1;
	UINT m_uMouseShowCrosshair;
	UINT m_uMouseRestrictToWindow;

	enum CPMCHOICE {CPM_SLOT4=0, CPM_SLOT5, CPM_UNPLUGGED, CPM_UNAVAILABLE, _CPM_MAX_CHOICES};
	TCHAR m_szCPMSlotChoices[_CPM_MAX_CHOICES * MaxMenuChoiceLen];
	CPMCHOICE m_CPMChoice; 
	CPMCHOICE m_CPMComboItemToChoice[_CPM_MAX_CHOICES];

	enum FOURPLAYCHOICE {FOURPLAY_SLOT3=0, FOURPLAY_SLOT4, FOURPLAY_SLOT5, FOURPLAY_UNPLUGGED, FOURPLAY_UNAVAILABLE, _FOURPLAY_MAX_CHOICES};
	TCHAR m_szFourPlaySlotChoices[_FOURPLAY_MAX_CHOICES * MaxMenuChoiceLen];
	FOURPLAYCHOICE m_FourPlayChoice;
	FOURPLAYCHOICE m_FourPlayComboItemToChoice[_FOURPLAY_MAX_CHOICES];

	enum SNESMAXCHOICE {SNESMAX_SLOT3=0, SNESMAX_SLOT4, SNESMAX_SLOT5, SNESMAX_UNPLUGGED, SNESMAX_UNAVAILABLE, _SNESMAX_MAX_CHOICES};
	TCHAR m_szSNESMAXSlotChoices[_SNESMAX_MAX_CHOICES * MaxMenuChoiceLen];
	SNESMAXCHOICE m_SNESMAXChoice;
	SNESMAXCHOICE m_SNESMAXComboItemToChoice[_SNESMAX_MAX_CHOICES];
};
