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
		m_PropertySheetHelper(PropertySheetHelper)
	{
		CPageSound::ms_this = this;
	}
	virtual ~CPageSound(){}

	static INT_PTR CALLBACK DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	uint32_t GetVolumeMax(void){ return VOLUME_MAX; }

protected:
	// IPropertySheetPage
	virtual INT_PTR DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	virtual void DlgOK(HWND hWnd);
	virtual void DlgCANCEL(HWND hWnd){}

private:
	enum SOUNDCARDCHOICE { SC_MOCKINGBOARD = 0, SC_PHASOR, SC_SAM, SC_EMPTY, SC_MEGAAUDIO, SC_SDMUSIC };

	void InitOptions(HWND hWnd);
	SOUNDCARDCHOICE CardTypeToComboItem(SS_CARDTYPE card);

	static CPageSound* ms_this;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;
	static const TCHAR m_soundchoices[];
	static const char m_soundCardChoices[];
	static const char m_soundCardChoicesEx[];
	static const char m_soundCardChoice_Unavailable[];
};
