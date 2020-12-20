#include "StdAfx.h"

#include "Interface.h"
#include "linux/duplicates/PropertySheet.h"

HINSTANCE g_hInstance          = (HINSTANCE)0;

void SetLoadedSaveStateFlag(bool)
{
}

IPropertySheet& GetPropertySheet()
{
  static CPropertySheet sg_PropertySheet;
  return sg_PropertySheet;
}
