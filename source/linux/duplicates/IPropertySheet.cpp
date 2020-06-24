#include "StdAfx.h"

#include "Common.h"
#include "Configuration/IPropertySheet.h"

void IPropertySheet::ApplyNewConfig(CConfigNeedingRestart const&, CConfigNeedingRestart const&)
{
}

UINT IPropertySheet::GetTheFreezesF8Rom(void)
{
  return 0;
}

void IPropertySheet::ConfigSaveApple2Type(eApple2Type apple2Type)
{
}
