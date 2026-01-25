#pragma once

#include "IPropertySheetPage.h"
#include <string>

class CPageConfigTfe : private IPropertySheetPage
{
public:
	CPageConfigTfe()
	{
		CPageConfigTfe::ms_this = this;
		m_tfe_virtual_dns = false;
		m_enableVirtualDnsCheckbox = false;
	}
	virtual ~CPageConfigTfe(){}

	static INT_PTR CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	std::string m_tfe_interface_name;
	bool m_tfe_virtual_dns;
	bool m_enableVirtualDnsCheckbox;

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window);
	virtual void DlgCANCEL(HWND window);
	virtual void ApplyConfigAfterClose() {}
	virtual void ResetToDefault() {}

private:
	BOOL get_tfename(int number, std::string & name, std::string & description);
	void gray_ungray_items(HWND hwnd);
	void init_tfe_dialog(HWND hwnd);
	void save_tfe_dialog(HWND hwnd);

	static CPageConfigTfe* ms_this;
};
