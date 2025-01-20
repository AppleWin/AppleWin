#include "guiddef.h"
#include "winerror.h"

HRESULT IUnknown::QueryInterface(int riid, void **ppvObject)
{
    return S_OK;
}

HRESULT IUnknown::Release()
{
    return S_OK;
}

HRESULT IAutoRelease::Release()
{
    const HRESULT res = IUnknown::Release();
    delete this;
    return res;
}

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
    return S_OK;
}
