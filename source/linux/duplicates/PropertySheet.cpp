#include "StdAfx.h"

#include "linux/duplicates/PropertySheet.h"

void CPropertySheet::Init(void)
{
}

DWORD CPropertySheet::GetVolumeMax(void)
{
  return 99;
}

bool CPropertySheet::SaveStateSelectImage(HWND hWindow, bool bSave)
{
  return false;
}

void CPropertySheet::ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld)
{
  m_PropertySheetHelper.ApplyNewConfig(ConfigNew, ConfigOld);
}

void CPropertySheet::ApplyNewConfigFromSnapshot(const CConfigNeedingRestart& ConfigNew)
{
  m_PropertySheetHelper.ApplyNewConfigFromSnapshot(ConfigNew);
}

void CPropertySheet::ConfigSaveApple2Type(eApple2Type apple2Type)
{
}

UINT CPropertySheet::GetScrollLockToggle(void)
{
  return 0;
}

void CPropertySheet::SetScrollLockToggle(UINT uValue)
{
}

UINT CPropertySheet::GetJoystickCursorControl(void)
{
  return 0;
}

void CPropertySheet::SetJoystickCursorControl(UINT uValue)
{
}

UINT CPropertySheet::GetJoystickCenteringControl(void)
{
  return 0;
}

void CPropertySheet::SetJoystickCenteringControl(UINT uValue)
{
}

UINT CPropertySheet::GetAutofire(UINT uButton)
{
  return 0;
}

void CPropertySheet::SetAutofire(UINT uValue)
{
}

bool CPropertySheet::GetButtonsSwapState(void)
{
  return false;
}

void CPropertySheet::SetButtonsSwapState(bool value)
{
}

UINT CPropertySheet::GetMouseShowCrosshair(void)
{
  return 0;
}

void CPropertySheet::SetMouseShowCrosshair(UINT uValue)
{
}

UINT CPropertySheet::GetMouseRestrictToWindow(void)
{
  return 0;
}

void CPropertySheet::SetMouseRestrictToWindow(UINT uValue)
{
}

UINT CPropertySheet::GetTheFreezesF8Rom(void)
{
  return 0;
}

void CPropertySheet::SetTheFreezesF8Rom(UINT uValue)
{
}
