#include "dmusicc.h"
#include "winerror.h"

HRESULT IReferenceClock::GetTime(REFERENCE_TIME *pTime)
{
  return S_OK;
}

HRESULT IReferenceClock::AdvisePeriodic(REFERENCE_TIME startTime, REFERENCE_TIME periodTime, HSEMAPHORE hSemaphore, DWORD *pdwAdviseCookie)
{
  return S_OK;
}

HRESULT IReferenceClock::Unadvise(DWORD dwAdviseCookie)
{
  return S_OK;
}
