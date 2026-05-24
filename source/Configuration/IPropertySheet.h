#pragma once

#include "Common.h"

class CConfigNeedingRestart;

class IPropertySheet
{
public:
	virtual void Init() = 0;
	virtual uint32_t GetVolumeMax() = 0;								// TODO:TC: Move out of here
	virtual bool SaveStateSelectImage(HWND hWindow, bool bSave) = 0;	// TODO:TC: Move out of here
	virtual void ResetAllToDefault() = 0;
	virtual void ApplyConfigAfterClose(UINT bmPages) = 0;
	virtual void ApplyNewConfigFromSnapshot() = 0;
	virtual void ConfigSaveApple2Type(eApple2Type apple2Type) = 0;

	virtual UINT GetScrollLockToggle() = 0;
	virtual void SetScrollLockToggle(UINT uValue) = 0;
	virtual UINT GetJoystickCursorControl() = 0;
	virtual void SetJoystickCursorControl(UINT uValue) = 0;
	virtual UINT GetJoystickCenteringControl() = 0;
	virtual void SetJoystickCenteringControl(UINT uValue) = 0;
	virtual UINT GetAutofire(UINT uButton) = 0;
	virtual UINT GetAutofire() = 0;
	virtual void SetAutofire(UINT uValue) = 0;
	virtual bool GetButtonsSwapState() = 0;
	virtual void SetButtonsSwapState(bool value) = 0;
	virtual UINT GetMouseShowCrosshair() = 0;
	virtual void SetMouseShowCrosshair(UINT uValue) = 0;
	virtual UINT GetMouseRestrictToWindow() = 0;
	virtual void SetMouseRestrictToWindow(UINT uValue) = 0;
	virtual UINT GetTheFreezesF8Rom() = 0;
	virtual void SetTheFreezesF8Rom(UINT uValue) = 0;
};
