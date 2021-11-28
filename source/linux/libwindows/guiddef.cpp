#include "guiddef.h"
#include "winerror.h"

IUnknown::~IUnknown()
{
}

HRESULT IUnknown::QueryInterface(int riid, void **ppvObject)
{
  return S_OK;
}

HRESULT IUnknown::Release()
{
  delete this;
  return S_OK;
}

HRESULT CoCreateInstance(
  REFCLSID  rclsid,
  LPUNKNOWN pUnkOuter,
  DWORD     dwClsContext,
  REFIID    riid,
  LPVOID    *ppv
)
{
  return S_OK;
}
