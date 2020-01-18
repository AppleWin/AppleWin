#pragma once

class CConfigNeedingRestart;

__interface IPropertySheet
{
	void Init(void);
	DWORD GetVolumeMax(void);								// TODO:TC: Move out of here
	bool SaveStateSelectImage(HWND hWindow, bool bSave);	// TODO:TC: Move out of here
	void ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld);
	void ConfigSaveApple2Type(eApple2Type apple2Type);

	UINT GetScrollLockToggle(void);
	void SetScrollLockToggle(UINT uValue);
	UINT GetJoystickCursorControl(void);
	void SetJoystickCursorControl(UINT uValue);
	UINT GetJoystickCenteringControl(void);
	void SetJoystickCenteringControl(UINT uValue);
	UINT GetAutofire(UINT uButton);
	void SetAutofire(UINT uValue);
	bool GetButtonsSwapState(void);
	void SetButtonsSwapState(bool value);
	UINT GetMouseShowCrosshair(void);
	void SetMouseShowCrosshair(UINT uValue);
	UINT GetMouseRestrictToWindow(void);
	void SetMouseRestrictToWindow(UINT uValue);
	UINT GetTheFreezesF8Rom(void);
	void SetTheFreezesF8Rom(UINT uValue);
};
