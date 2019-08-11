#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
#include "Card.h"

class CPropertySheetHelper;

class CPageSound : private IPropertySheetPage
{
public:
	CPageSound(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_SOUND),
		m_PropertySheetHelper(PropertySheetHelper),
		m_NewCardType(CT_Empty),
		m_nCurrentIDCheckButton(0)
	{
		CPageSound::ms_this = this;
	}
	virtual ~CPageSound(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	DWORD GetVolumeMax(void){ return VOLUME_MAX; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	void InitOptions(HWND hWnd);
	bool NewSoundcardConfigured(HWND hWnd, WPARAM wparam, SS_CARDTYPE NewCardType);

	static CPageSound* ms_this;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;
	static const TCHAR m_soundchoices[];

	SS_CARDTYPE m_NewCardType;
	int m_nCurrentIDCheckButton;
};
