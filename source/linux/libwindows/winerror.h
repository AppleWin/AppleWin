#pragma once

#define _HRESULT_TYPEDEF_(x) ((HRESULT)x)

#define CLASS_E_NOAGGREGATION _HRESULT_TYPEDEF_(0x80040110)

#define E_NOINTERFACE HRESULT(0x80004002)

#define E_NOTIMPL _HRESULT_TYPEDEF_(0x80004001)
#define E_FAIL _HRESULT_TYPEDEF_(0x80004005)
#define E_OUTOFMEMORY _HRESULT_TYPEDEF_(0x8007000E)
#define E_INVALIDARG _HRESULT_TYPEDEF_(0x80070057)

#define S_OK HRESULT(0)
#define S_FALSE HRESULT(1)

#define MAKE_HRESULT(sev, fac, code)                                                                                   \
    ((HRESULT)(((unsigned int)(sev) << 31) | ((unsigned int)(fac) << 16) | ((unsigned int)(code))))
#define MAKE_SCODE(sev, fac, code)                                                                                     \
    ((SCODE)(((unsigned int)(sev) << 31) | ((unsigned int)(fac) << 16) | ((unsigned int)(code))))
#define SUCCEEDED(stat) ((HRESULT)(stat) >= 0)
#define FAILED(stat) ((HRESULT)(stat) < 0)

#define HRESULT_CODE(hr) ((hr) & 0xFFFF)
#define SCODE_CODE(sc) ((sc) & 0xFFFF)
