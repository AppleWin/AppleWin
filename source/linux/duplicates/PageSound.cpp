#include "StdAfx.h"

#include "Configuration/PageSound.h"

// IPropertySheetPage
INT_PTR CPageSound::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    return 0;
}

void CPageSound::DlgOK(HWND hWnd)
{
}

CPageSound *CPageSound::ms_this = nullptr;
