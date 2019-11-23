#include "linux/windows/handles.h"

BOOL CloseHandle(HANDLE hObject)
{
  if (hObject && hObject != INVALID_HANDLE_VALUE)
  {
    delete hObject;
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}
