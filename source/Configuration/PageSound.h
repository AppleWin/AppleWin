#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "PageConfigTfe.h"
#include "Card.h"

class CPropertySheetHelper;

class CPageSound : private IPropertySheetPage
{
public:
	CPageSound(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_SOUND),
		m_PropertySheetHelper(PropertySheetHelper),
		m_mouseShowCrosshair(0),
		m_mouseRestrictToWindow(0)
	{
		CPageSound::ms_this = this;
		ms_slot = NUM_SLOTS;	// invalid
	}
	virtual ~CPageSound(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	UINT GetMouseShowCrosshair(void) { return m_mouseShowCrosshair; }
	void SetMouseShowCrosshair(UINT uValue) { m_mouseShowCrosshair = uValue; }
	UINT GetMouseRestrictToWindow(void) { return m_mouseRestrictToWindow; }
	void SetMouseRestrictToWindow(UINT uValue) { m_mouseRestrictToWindow = uValue; }

	uint32_t GetVolumeMax(void){ return VOLUME_MAX; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	enum AUXCARDCHOICE { SC_80COL = 0, SC_EXT80COL, SC_RAMWORKS, SC_AUX_EMPTY };

	void InitOptions(HWND hWnd);
	AUXCARDCHOICE AuxCardTypeToComboItem(SS_CARDTYPE card);
	int CardTypeToComboItem(UINT slot);

	static INT_PTR CALLBACK DlgProcDisk2(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcDisk2Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcHarddisk(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcHarddiskInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcSSC(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcSSCInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcPrinter(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcPrinterInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcMouseCard(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcMouseCardInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcRamWorks3(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcRamWorks3Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	void InitComboFloppyDrive(HWND hWnd, UINT slot);
	void HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot);
	void EnableFloppyDrive(HWND hWnd, BOOL enable);
	void HandleFloppyDriveSwap(HWND hWnd, UINT slot);

	void InitComboHDD(HWND hWnd, UINT slot);
	void HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot);
	void EnableHDD(HWND hWnd, BOOL enable);
	void HandleHDDSwap(HWND hWnd, UINT slot);

	void DlgDisk2OK(HWND hWnd);
	void DlgHarddiskOK(HWND hWnd);
	void DlgPrinterOK(HWND hWnd);
	void DlgMouseCardOK(HWND hWnd);
	void DlgRamWorks3OK(HWND hWnd);

	UINT RemovalConfirmation(UINT command);

	static CPageSound* ms_this;
	static UINT ms_slot;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	CPageConfigTfe m_PageConfigTfe;

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;
	static const char m_auxChoices[];

	static const char m_defaultDiskOptions[];
	static const char m_defaultHDDOptions[];

	std::vector<SS_CARDTYPE> m_choicesList[NUM_SLOTS];

	UINT m_mouseShowCrosshair;
	UINT m_mouseRestrictToWindow;
};
