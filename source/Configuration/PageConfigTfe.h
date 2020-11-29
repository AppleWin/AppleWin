#pragma once

#include "IPropertySheetPage.h"
#include "../Tfe/Uilib.h"

class CPageConfigTfe : private IPropertySheetPage
{
public:
	CPageConfigTfe()
	{
		CPageConfigTfe::ms_this = this;
	}
	virtual ~CPageConfigTfe(){}

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window);
	virtual void DlgCANCEL(HWND window);

private:
	BOOL get_tfename(int number, char **ppname, char **ppdescription);
	int gray_ungray_items(HWND hwnd);
	void init_tfe_dialog(HWND hwnd);
	void save_tfe_dialog(HWND hwnd);

	static CPageConfigTfe* ms_this;
	static uilib_localize_dialog_param ms_dialog[];
	static uilib_dialog_group ms_leftgroup[];
	static uilib_dialog_group ms_rightgroup[];
};
