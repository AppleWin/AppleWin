#pragma once

#include "PropertySheetHelper.h"
#include "PageConfig.h"
#include "PageInput.h"
#include "PageSound.h"
#include "PageDisk.h"
#include "PageAdvanced.h"

extern class CPropertySheet sg_PropertySheet;

class CPropertySheet
{
public:
	CPropertySheet() :
		g_PageConfig(g_PropertySheetHelper),
		g_PageInput(g_PropertySheetHelper),
		g_PageSound(g_PropertySheetHelper),
		g_PageDisk(g_PropertySheetHelper),
		g_PageAdvanced(g_PropertySheetHelper)
	{}
	virtual ~CPropertySheet(){}

	void Init(void);
	DWORD GetVolumeMax(void);								// TODO:TC: Move out of here
	bool SaveStateSelectImage(HWND hWindow, bool bSave);	// TODO:TC: Move out of here

	UINT GetScrollLockToggle(void){ return g_PageInput.GetScrollLockToggle(); }
	void SetScrollLockToggle(UINT uValue){ g_PageInput.SetScrollLockToggle(uValue); }
	UINT GetMouseShowCrosshair(void){ return g_PageInput.GetMouseShowCrosshair(); }
	void SetMouseShowCrosshair(UINT uValue){ g_PageInput.SetMouseShowCrosshair(uValue); }
	UINT GetMouseRestrictToWindow(void){ return g_PageInput.GetMouseRestrictToWindow(); }
	void SetMouseRestrictToWindow(UINT uValue){ g_PageInput.SetMouseRestrictToWindow(uValue); }
	UINT GetTheFreezesF8Rom(void){ return g_PageAdvanced.GetTheFreezesF8Rom(); }
	void SetTheFreezesF8Rom(UINT uValue){ g_PageAdvanced.SetTheFreezesF8Rom(uValue); }

private:
	CPropertySheetHelper g_PropertySheetHelper;
	CPageConfig g_PageConfig;
	CPageInput g_PageInput;
	CPageSound g_PageSound;
	CPageDisk g_PageDisk;
	CPageAdvanced g_PageAdvanced;
};
