#include "StdAfx.h"

#include "Configuration/PageAdvanced.h"

// IPropertySheetPage
INT_PTR CPageAdvanced::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  return 0;
}

void CPageAdvanced::DlgOK(HWND hWnd)
{

}

CPageAdvanced* CPageAdvanced::ms_this = nullptr;
