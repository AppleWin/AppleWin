#pragma once

#include "Common.h"
#include "Configuration/IPropertySheet.h"
#include "Configuration/PropertySheetHelper.h"

class CConfigNeedingRestart;

class CPropertySheet : public IPropertySheet
{
public:
  void Init(void) override;
  DWORD GetVolumeMax(void) override;
  bool SaveStateSelectImage(HWND hWindow, bool bSave) override;
  void ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld) override;
  void ApplyNewConfigFromSnapshot(const CConfigNeedingRestart& ConfigNew) override;
  void ConfigSaveApple2Type(eApple2Type apple2Type) override;
  UINT GetScrollLockToggle(void) override;
  void SetScrollLockToggle(UINT uValue) override;
  UINT GetJoystickCursorControl(void) override;
  void SetJoystickCursorControl(UINT uValue) override;
  UINT GetJoystickCenteringControl(void) override;
  void SetJoystickCenteringControl(UINT uValue) override;
  UINT GetAutofire(UINT uButton) override;
  void SetAutofire(UINT uValue) override;
  bool GetButtonsSwapState(void) override;
  void SetButtonsSwapState(bool value) override;
  UINT GetMouseShowCrosshair(void) override;
  void SetMouseShowCrosshair(UINT uValue) override;
  UINT GetMouseRestrictToWindow(void) override;
  void SetMouseRestrictToWindow(UINT uValue) override;
  UINT GetTheFreezesF8Rom(void) override;
  void SetTheFreezesF8Rom(UINT uValue) override;
private:
  CPropertySheetHelper m_PropertySheetHelper;
};
