#pragma once

#include "linux/windows/wincompat.h"
#include "linux/windows/handles.h"

#include <string>

typedef struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
} RGBQUAD;

#define RGB(r,g,b)          ((COLORREF)((BYTE)(r) | ((BYTE)(g) << 8) | ((BYTE)(b) << 16)))
#define BI_RGB           0

typedef HANDLE HBITMAP;
typedef HANDLE HGDIOBJ;

// Bitmap
HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename);
LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits);

BOOL DeleteObject(HGDIOBJ ho);
