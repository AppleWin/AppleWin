#include "StdAfx.h"

#include "Configuration/PageConfigTfe.h"

// IPropertySheetPage
INT_PTR CPageConfigTfe::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

void CPageConfigTfe::DlgOK(HWND hWnd)
{
}

void CPageConfigTfe::DlgCANCEL(HWND window)
{
}

CPageConfigTfe *CPageConfigTfe::ms_this = nullptr;
