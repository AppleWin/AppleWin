#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class CPropertySheetHelper;

class CPageInput : public IPropertySheetPage
{
public:
	CPageInput(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_INPUT),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uAfterClose(0),
		m_MousecardSlotChange(CARD_UNCHANGED),
		m_CPMcardSlotChange(CARD_UNCHANGED),
		m_uScrollLockToggle(0),
		m_uMouseShowCrosshair(0),
		m_uMouseRestrictToWindow(0),
		m_CPMChoice(CPM_UNPLUGGED)
	{
		CPageInput::ms_this = this;
	}
	virtual ~CPageInput(){}

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetScrollLockToggle(void){ return m_uScrollLockToggle; }
	void SetScrollLockToggle(UINT uValue){ m_uScrollLockToggle = uValue; }
	UINT GetMouseShowCrosshair(void){ return m_uMouseShowCrosshair; }
	void SetMouseShowCrosshair(UINT uValue){ m_uMouseShowCrosshair = uValue; }
	UINT GetMouseRestrictToWindow(void){ return m_uMouseRestrictToWindow; }
	void SetMouseRestrictToWindow(UINT uValue){ m_uMouseRestrictToWindow = uValue; }

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window);
	virtual void DlgCANCEL(HWND window){}

private:
	void InitJoystickChoices(HWND window, int nJoyNum, int nIdcValue);
	void InitCPMChoices(HWND window);

	static CPageInput* ms_this;
	static const UINT MaxMenuChoiceLen = 40;

	enum JOY0CHOICE {J0C_DISABLED=0, J0C_JOYSTICK1, J0C_KEYBD_STANDARD, J0C_KEYBD_CENTERING, J0C_MOUSE, J0C_MAX};
	enum JOY1CHOICE {J1C_DISABLED=0, J1C_JOYSTICK2, J1C_KEYBD_STANDARD, J1C_KEYBD_CENTERING, J1C_MOUSE, J1C_MAX};
	static const TCHAR m_szJoyChoice0[];
	static const TCHAR m_szJoyChoice1[];
	static const TCHAR m_szJoyChoice2[];
	static const TCHAR m_szJoyChoice3[];
	static const TCHAR m_szJoyChoice4[];
	static const TCHAR m_szJoyChoice5[];
	static const TCHAR* const m_pszJoy0Choices[J0C_MAX];
	static const TCHAR* const m_pszJoy1Choices[J1C_MAX];

	static const TCHAR m_szCPMSlotChoice_Slot4[];
	static const TCHAR m_szCPMSlotChoice_Slot5[];
	static const TCHAR m_szCPMSlotChoice_Unplugged[];
	static const TCHAR m_szCPMSlotChoice_Unavailable[];

	int m_nJoy0ChoiceTranlationTbl[J0C_MAX];
	TCHAR m_joystick0choices[J0C_MAX * MaxMenuChoiceLen];
	int m_nJoy1ChoiceTranlationTbl[J1C_MAX];
	TCHAR m_joystick1choices[J1C_MAX * MaxMenuChoiceLen];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uAfterClose;

	CARDSTATE m_MousecardSlotChange;
	CARDSTATE m_CPMcardSlotChange;
	UINT m_uScrollLockToggle;
	UINT m_uMouseShowCrosshair;
	UINT m_uMouseRestrictToWindow;

	enum CPMCHOICE {CPM_SLOT4=0, CPM_SLOT5, CPM_UNPLUGGED, CPM_UNAVAILABLE, _CPM_MAX_CHOICES};
	TCHAR m_szCPMSlotChoices[_CPM_MAX_CHOICES * MaxMenuChoiceLen];
	CPMCHOICE m_CPMChoice; 
	CPMCHOICE m_CPMComboItemToChoice[_CPM_MAX_CHOICES];
};
