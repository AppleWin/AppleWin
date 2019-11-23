#include "linux/windows/bitmap.h"

BOOL DeleteObject(HGDIOBJ ho)
{
  return CloseHandle(ho);
}
