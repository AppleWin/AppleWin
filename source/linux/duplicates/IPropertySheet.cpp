#include "StdAfx.h"

#include "Common.h"
#include "Configuration/IPropertySheet.h"
#include "Configuration/PropertySheetHelper.h"

void IPropertySheet::ApplyNewConfig(CConfigNeedingRestart const & newConfig, CConfigNeedingRestart const & oldConfig)
{
  CPropertySheetHelper helper;
  helper.ApplyNewConfig(newConfig, oldConfig);
}

UINT IPropertySheet::GetTheFreezesF8Rom(void)
{
  return 0;
}

void IPropertySheet::ConfigSaveApple2Type(eApple2Type apple2Type)
{
}

void IPropertySheet::SetTheFreezesF8Rom(UINT uValue)
{
}

void IPropertySheet::SetJoystickCursorControl(UINT uValue)
{
}

void IPropertySheet::SetMouseRestrictToWindow(UINT uValue)
{
}

void IPropertySheet::SetScrollLockToggle(UINT uValue)
{
}

void IPropertySheet::SetJoystickCenteringControl(UINT uValue)
{
}

DWORD IPropertySheet::GetVolumeMax(void)
{
  return 99;
}

void IPropertySheet::SetButtonsSwapState(bool value)
{
}

void IPropertySheet::SetAutofire(UINT uValue)
{
}

void IPropertySheet::SetMouseShowCrosshair(UINT uValue)
{
}
