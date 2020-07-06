#pragma once

#include "linux/windows/winbase.h"
#include "linux/windows/handles.h"
#include "linux/windows/guiddef.h"

typedef LONGLONG REFERENCE_TIME;
struct IReferenceClock : public IUnknown
{
  HRESULT GetTime(REFERENCE_TIME *pTime);
  HRESULT AdvisePeriodic(REFERENCE_TIME startTime, REFERENCE_TIME periodTime, HSEMAPHORE hSemaphore, DWORD *pdwAdviseCookie);
  HRESULT Unadvise(DWORD dwAdviseCookie);
};
