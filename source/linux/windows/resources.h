#pragma once

#include "linux/windows/wincompat.h"
#include "linux/windows/handles.h"

#include <string>
#include <vector>

struct HRSRC : public CHANDLE
{
  std::vector<char> data;
  HRSRC(const void * = NULL)
  {
  }
  operator const void * () const
  {
    return data.data();
  }
};

const char * MAKEINTRESOURCE(int x);
HRSRC FindResource(void *, const std::string & filename, const char *);
DWORD SizeofResource(void *, const HRSRC &);
HGLOBAL LoadResource(void *, HRSRC &);
BYTE * LockResource(HGLOBAL);
