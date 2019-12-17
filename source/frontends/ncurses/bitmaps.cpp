#include "StdAfx.h"
#include "Log.h"

HBITMAP LoadBitmap(HINSTANCE hInstance, const std::string & filename)
{
  LogFileOutput("LoadBitmap: not loading resource %s\n", filename.c_str());
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  memset(lpvBits, 0, cb);
  return cb;
}
