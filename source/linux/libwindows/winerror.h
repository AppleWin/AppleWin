#pragma once

#include "wincompat.h"

#define FAILED(stat) ((HRESULT)(stat)<0)
#define E_NOINTERFACE HRESULT(0x80004002)
#define S_OK HRESULT(0)
#define S_FALSE HRESULT(1)
#define SEVERITY_SUCCESS    0
#define SEVERITY_ERROR      1

#define MAKE_HRESULT(sev,fac,code)                                      \
    ((HRESULT) (((unsigned int)(sev)<<31) | ((unsigned int)(fac)<<16) | ((unsigned int)(code))) )
#define MAKE_SCODE(sev,fac,code) \
        ((SCODE) (((unsigned int)(sev)<<31) | ((unsigned int)(fac)<<16) | ((unsigned int)(code))) )
#define SUCCEEDED(stat) ((HRESULT)(stat)>=0)
#define FAILED(stat) ((HRESULT)(stat)<0)
#define IS_ERROR(stat) (((unsigned int)(stat)>>31) == SEVERITY_ERROR)

#define HRESULT_CODE(hr) ((hr) & 0xFFFF)
#define SCODE_CODE(sc)   ((sc) & 0xFFFF)
