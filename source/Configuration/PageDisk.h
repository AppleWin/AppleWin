#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class CPropertySheetHelper;

class CPageDisk : private IPropertySheetPage
{
public:
	CPageDisk(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_DISK),
		m_PropertySheetHelper(PropertySheetHelper)
	{
		CPageDisk::ms_this = this;
	}
	virtual ~CPageDisk(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	void InitComboFloppyDrive(HWND hWnd, UINT slot);
	void InitComboHDD(HWND hWnd, UINT slot);
	void EnableHDD(HWND hWnd, BOOL bEnable);
	void EnableFloppyDrive(HWND hWnd, BOOL bEnable, UINT slot);
	void HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected);
	void HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT comboOther, UINT slot);
	void HandleHDDSwap(HWND hWnd);
	UINT RemovalConfirmation(UINT uCommand);

	static CPageDisk* ms_this;
	static const TCHAR m_defaultDiskOptions[];
	static const TCHAR m_defaultHDDOptions[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
};
