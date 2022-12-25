#pragma once

#include "wincompat.h"

typedef struct joyinfoex_tag {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwXpos;
  DWORD dwYpos;
  DWORD dwZpos;
  DWORD dwRpos;
  DWORD dwUpos;
  DWORD dwVpos;
  DWORD dwButtons;
  DWORD dwButtonNumber;
  DWORD dwPOV;
  DWORD dwReserved1;
  DWORD dwReserved2;
} JOYINFOEX, *PJOYINFOEX, *NPJOYINFOEX, *LPJOYINFOEX;
