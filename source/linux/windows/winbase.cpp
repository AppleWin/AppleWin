#include "linux/windows/winbase.h"

#include <errno.h>

DWORD       WINAPI GetLastError(void)
{
  return errno;
}
