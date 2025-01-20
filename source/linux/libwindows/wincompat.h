/****************************************************************************
 *                                                                           *
 * wincompat.h -- Basic Windows Type Definitions                                *
 *                                                                           *
 * Copyright (c) 1985-1997, Microsoft Corp. All rights reserved.             *
 *                                                                           *
 ****************************************************************************/

/*
        Please note all long types (save for pointers) were replaced by int types
        for x64 systems support!

                On x32 long type takes 4 bytes, on x64 long type tekes 8 bytes)
        -- Krez beotiger
*/

#ifndef _WINDEF_
#define _WINDEF_

#include <cstdint>
#include <cassert>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef BASETYPES
#define BASETYPES
    typedef uint32_t ULONG; // 32 bit long in VS
    typedef uint16_t USHORT;
    typedef unsigned char UCHAR;
#endif /* !BASETYPES */

    typedef int16_t INT16;
    typedef uint16_t UINT16;

// in Visual Studio, __int64 is *always* the same as long long int
// in 32 *and* 64 bit
// static_assert(std::is_same<__int64, long long int>::value);
//
// wine defines it as long (64 bit) or long long (32 bit)
//
// we stick to VS's definition
// this is important when selecting the correct printf format specifier
#define __int64 long long int
    typedef __int64 LONGLONG;
    typedef unsigned __int64 UINT64;

    typedef intptr_t INT_PTR;
    typedef uintptr_t UINT_PTR;

    typedef uint32_t UINT32;

#define MAX_PATH 260

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define FAR

#ifndef CONST
#define CONST const
#endif

    typedef unsigned int DWORD;
    typedef int BOOL;
    typedef unsigned char BYTE;
    typedef unsigned short WORD;
    typedef BOOL *LPBOOL;
    typedef BYTE *LPBYTE;
    typedef WORD *LPWORD;
    typedef DWORD *LPDWORD;
    typedef void *LPVOID;
    typedef CONST void *LPCVOID;

    typedef int INT;
    typedef unsigned int UINT;

#define MAKEWORD(a, b) ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l) ((WORD)(l))
#define HIWORD(l) ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w) ((BYTE)(w))
#define HIBYTE(w) ((BYTE)(((WORD)(w) >> 8) & 0xFF))

    typedef DWORD COLORREF;
    typedef DWORD *LPCOLORREF;

////////////////////////// WINNT ///////////////////////////////
#define VOID void
    typedef char CHAR;
    typedef short SHORT;
    typedef int /*long*/ LONG;
    typedef LONG *PLONG;
    typedef wchar_t WCHAR;

    typedef LONG HRESULT;
    typedef intptr_t LONG_PTR;
    typedef uintptr_t ULONG_PTR;
    typedef LONG_PTR LRESULT;
    typedef ULONG_PTR DWORD_PTR;

    typedef DWORD LCID;

    //
    // ANSI (Multi-byte Character) types
    //
    typedef CHAR *PCHAR;

    typedef CHAR *LPSTR;
    typedef CONST CHAR *LPCSTR;

    typedef LPSTR PTSTR, LPTSTR;
    typedef LPCSTR LPCTSTR;

    typedef WCHAR *LPWSTR;
    typedef CONST WCHAR *LPCWSTR;

#ifndef _TCHAR_DEFINED
// TCHAR a typedef or a define?
// a define for the single reason that
// othwerise QTCreator does not show the values (i.e. string) while debugging
// it treats it as an array of bytes
#define TCHAR char
    typedef TCHAR _TCHAR;
#define _TCHAR_DEFINED
#endif /* !_TCHAR_DEFINED */

// TCHAR support
#define __TEXT(quote) quote       // r_winnt
#define TEXT(quote) __TEXT(quote) // r_winnt

#define WINAPI
#define __stdcall
#define CALLBACK

#ifdef _DEBUG
#define _ASSERT(expr) assert(expr)
#else
#define _ASSERT(expr)
#endif

#define __interface struct
#define __forceinline inline

#define _tmain main

    typedef void *HWND;
    typedef LONG_PTR LPARAM;
    typedef UINT_PTR WPARAM;

    typedef int errno_t;

#ifdef __cplusplus
}
#endif

#endif /* _WINDEF_ */
