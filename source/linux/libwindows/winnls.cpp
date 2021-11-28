#include "winnls.h"

#include <cstdlib>
#include <cstring>

// massive hack until I understand how it works!

INT WINAPI MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, INT cbMultiByte, LPWSTR lpWideCharStr, INT cchWideChar)
{
//  int res = mbstowcs(lpWideCharStr, lpMultiByteStr, strlen(lpMultiByteStr));
  if (lpWideCharStr)
  {
    strcpy(reinterpret_cast<char *>(lpWideCharStr), lpMultiByteStr);
  }
  return strlen(lpMultiByteStr) + 1;  // add NULL char at end
}

INT WINAPI WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, INT cchWideChar, LPSTR lpMultiByteStr, INT cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
  if (lpMultiByteStr)
  {
    strcpy(lpMultiByteStr, reinterpret_cast<const char *>(lpWideCharStr));
  }
  //  size_t res = wcstombs(lpMultiByteStr, lpWideCharStr, cbMultiByte);
  return strlen((const char *)lpWideCharStr) + 1;
}
