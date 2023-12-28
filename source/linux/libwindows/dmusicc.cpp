#include "dmusicc.h"
#include "winerror.h"

HRESULT IReferenceClock::GetTime(REFERENCE_TIME *pTime)
{
  return S_OK;
}

HRESULT IReferenceClock::AdvisePeriodic(REFERENCE_TIME startTime, REFERENCE_TIME periodTime, HSEMAPHORE hSemaphore, DWORD_PTR *pdwAdviseCookie)
{
  return S_OK;
}

HRESULT IReferenceClock::Unadvise(DWORD_PTR dwAdviseCookie)
{
  return S_OK;
}
