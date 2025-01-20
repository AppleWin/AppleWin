#include "StdAfx.h"

#include "Configuration/PageDisk.h"

// IPropertySheetPage
INT_PTR CPageDisk::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

void CPageDisk::DlgOK(HWND hWnd)
{
}

CPageDisk *CPageDisk::ms_this = nullptr;
