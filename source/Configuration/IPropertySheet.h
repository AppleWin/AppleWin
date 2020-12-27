#pragma once

#include "Common.h"

class CConfigNeedingRestart;

class IPropertySheet
{
public:
	virtual void Init(void) = 0;
	virtual DWORD GetVolumeMax(void) = 0;								// TODO:TC: Move out of here
	virtual bool SaveStateSelectImage(HWND hWindow, bool bSave) = 0;	// TODO:TC: Move out of here
	virtual void ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld) = 0;
	virtual void ConfigSaveApple2Type(eApple2Type apple2Type) = 0;

	virtual UINT GetScrollLockToggle(void) = 0;
	virtual void SetScrollLockToggle(UINT uValue) = 0;
	virtual UINT GetJoystickCursorControl(void) = 0;
	virtual void SetJoystickCursorControl(UINT uValue) = 0;
	virtual UINT GetJoystickCenteringControl(void) = 0;
	virtual void SetJoystickCenteringControl(UINT uValue) = 0;
	virtual UINT GetAutofire(UINT uButton) = 0;
	virtual void SetAutofire(UINT uValue) = 0;
	virtual bool GetButtonsSwapState(void) = 0;
	virtual void SetButtonsSwapState(bool value) = 0;
	virtual UINT GetMouseShowCrosshair(void) = 0;
	virtual void SetMouseShowCrosshair(UINT uValue) = 0;
	virtual UINT GetMouseRestrictToWindow(void) = 0;
	virtual void SetMouseRestrictToWindow(UINT uValue) = 0;
	virtual UINT GetTheFreezesF8Rom(void) = 0;
	virtual void SetTheFreezesF8Rom(UINT uValue) = 0;
};
