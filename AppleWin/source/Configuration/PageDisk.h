#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class CPropertySheetHelper;

class CPageDisk : public IPropertySheetPage
{
public:
	CPageDisk(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_DISK),
		m_PropertySheetHelper(PropertySheetHelper)
	{
		CPageDisk::ms_this = this;
	}
	virtual ~CPageDisk(){}

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window, UINT afterclose);
	virtual void DlgCANCEL(HWND window){}

private:
	void EnableHDD(HWND window, BOOL bEnable);

	static CPageDisk* ms_this;
	static const TCHAR m_discchoices[];

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
};
