#pragma once

#include "wincompat.h"

#define CP_ACP        0
#define CP_UTF8       65001
#define MB_ERR_INVALID_CHARS        0x08

INT         WINAPI MultiByteToWideChar(UINT,DWORD,LPCSTR,INT,LPWSTR,INT);
INT         WINAPI WideCharToMultiByte(UINT,DWORD,LPCWSTR,INT,LPSTR,INT,LPCSTR,LPBOOL);
