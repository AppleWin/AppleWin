#pragma once

#include "wincompat.h"

typedef int GUID;
typedef GUID *LPGUID;
typedef GUID REFCLSID;
typedef GUID REFIID;

#define CLSCTX_INPROC 0
#define CLSID_SystemClock 1
#define IID_IReferenceClock 2

struct IUnknown
{
  HRESULT QueryInterface(int riid, void **ppvObject);
  virtual HRESULT Release();

  virtual ~IUnknown() = default;
};
typedef IUnknown *LPUNKNOWN;

struct IAutoRelease : public IUnknown
{
  virtual HRESULT Release() override;
};

HRESULT CoCreateInstance(
  REFCLSID  rclsid,
  LPUNKNOWN pUnkOuter,
  DWORD     dwClsContext,
  REFIID    riid,
  LPVOID    *ppv
);
