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
		ms_slot = NUM_SLOTS;	// invalid
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
//	enum SOUNDCARDCHOICE { SC_MOCKINGBOARD = 0, SC_PHASOR, SC_SAM, SC_EMPTY, SC_MEGAAUDIO, SC_SDMUSIC };
	enum AUXCARDCHOICE { SC_80COL = 0, SC_EXT80COL, SC_RAMWORKS, SC_AUX_EMPTY };

	void InitOptions(HWND hWnd);
//	SOUNDCARDCHOICE CardTypeToComboItem(SS_CARDTYPE card);
	AUXCARDCHOICE AuxCardTypeToComboItem(SS_CARDTYPE card);
	int CardTypeToComboItem(UINT slot);

	static INT_PTR CALLBACK DlgProcDisk2(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcDisk2Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	static INT_PTR CALLBACK DlgProcHarddisk(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);
	INT_PTR DlgProcHarddiskInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	void InitComboFloppyDrive(HWND hWnd, UINT slot);
	void HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot);
	void EnableFloppyDrive(HWND hWnd, BOOL enable);

	void InitComboHDD(HWND hWnd, UINT slot);
	void HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot);
	void EnableHDD(HWND hWnd, BOOL enable);

	UINT RemovalConfirmation(UINT command);

	static CPageSound* ms_this;
	static UINT ms_slot;

	const PAGETYPE m_Page;
	CPropertySheetHelper& m_PropertySheetHelper;

	static const UINT VOLUME_MIN = 0;
	static const UINT VOLUME_MAX = 59;
	static const char m_soundchoices[];
	static const char m_soundCardChoices[];
	static const char m_soundCardChoicesEx[];
	static const char m_soundCardChoice_Unavailable[];
	static const char m_auxChoices[];

	static const char m_defaultDiskOptions[];
	static const char m_defaultHDDOptions[];

	std::vector<SS_CARDTYPE> choicesList[NUM_SLOTS];
};
