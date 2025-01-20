#include "StdAfx.h"

#include "Configuration/PageConfig.h"

// IPropertySheetPage
INT_PTR CPageConfig::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

void CPageConfig::DlgOK(HWND hWnd)
{
}

CPageConfig *CPageConfig::ms_this = nullptr;
