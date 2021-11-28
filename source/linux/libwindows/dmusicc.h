#pragma once

#include "winbase.h"
#include "winhandles.h"
#include "guiddef.h"

typedef LONGLONG REFERENCE_TIME;
struct IReferenceClock : public IUnknown
{
  HRESULT GetTime(REFERENCE_TIME *pTime);
  HRESULT AdvisePeriodic(REFERENCE_TIME startTime, REFERENCE_TIME periodTime, HSEMAPHORE hSemaphore, DWORD *pdwAdviseCookie);
  HRESULT Unadvise(DWORD dwAdviseCookie);
};
