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
