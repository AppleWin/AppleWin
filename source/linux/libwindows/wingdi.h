#pragma once

#include "wincompat.h"

typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

#define RGB(r,g,b)          ((COLORREF)((BYTE)(r) | ((BYTE)(g) << 8) | ((BYTE)(b) << 16)))
#define BI_RGB           0
