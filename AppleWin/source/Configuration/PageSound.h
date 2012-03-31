#pragma once

#include "IPropertySheetPage.h"
#include "PropertySheetDefs.h"
class CPropertySheetHelper;

class CPageSound : public IPropertySheetPage
{
public:
	CPageSound(CPropertySheetHelper& PropertySheetHelper) :
		m_Page(PG_SOUND),
		m_PropertySheetHelper(PropertySheetHelper),
		m_uAfterClose(0),
		m_NewCardType(CT_Empty),
		m_SoundcardSlotChange(CARD_UNCHANGED),
		m_nCurrentIDCheckButton(0)
	{
		CPageSound::ms_this = this;
	}
	virtual ~CPageSound(){}

	static BOOL CALLBACK DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	DWORD GetVolumeMax(void){ return VOLUME_MAX; }
	bool NewSoundcardConfigured(HWND window, WPARAM wparam, LPCSTR pMsg);

protected:
	// IPropertySheetPage
	virtual BOOL DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND window);
	virtual void DlgCANCEL(HWND window){}

private:
	static CPageSound* ms_this;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;
	UINT m_uAfterClose;

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;
	static const TCHAR m_soundchoices[];

	SS_CARDTYPE m_NewCardType;
	CARDSTATE m_SoundcardSlotChange;
	int m_nCurrentIDCheckButton;
};
