#pragma once

#include "Common.h"
#include "Configuration/IPropertySheet.h"
#include "Configuration/PropertySheetHelper.h"

class CConfigNeedingRestart;

class CPropertySheet : public IPropertySheet
{
public:
  virtual void Init(void);
  virtual DWORD GetVolumeMax(void);
  virtual bool SaveStateSelectImage(HWND hWindow, bool bSave);
  virtual void ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld);
  virtual void ConfigSaveApple2Type(eApple2Type apple2Type);
  virtual UINT GetScrollLockToggle(void);
  virtual void SetScrollLockToggle(UINT uValue);
  virtual UINT GetJoystickCursorControl(void);
  virtual void SetJoystickCursorControl(UINT uValue);
  virtual UINT GetJoystickCenteringControl(void);
  virtual void SetJoystickCenteringControl(UINT uValue);
  virtual UINT GetAutofire(UINT uButton);
  virtual void SetAutofire(UINT uValue);
  virtual bool GetButtonsSwapState(void);
  virtual void SetButtonsSwapState(bool value);
  virtual UINT GetMouseShowCrosshair(void);
  virtual void SetMouseShowCrosshair(UINT uValue);
  virtual UINT GetMouseRestrictToWindow(void);
  virtual void SetMouseRestrictToWindow(UINT uValue);
  virtual UINT GetTheFreezesF8Rom(void);
  virtual void SetTheFreezesF8Rom(UINT uValue);
private:
  CPropertySheetHelper m_PropertySheetHelper;
};
