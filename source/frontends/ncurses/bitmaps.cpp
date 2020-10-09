#include "StdAfx.h"
#include "Log.h"

HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename)
{
  LogFileOutput("LoadBitmap: not loading resource %s\n", filename);
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  memset(lpvBits, 0, cb);
  return cb;
}
