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

	static BOOL CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	void EnableHDD(HWND hWnd, BOOL bEnable);

	static CPageDisk* ms_this;
	static const TCHAR m_discchoices[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
};
