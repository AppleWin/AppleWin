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
extern "C" {
#endif

#ifndef BASETYPES
#define BASETYPES
typedef uint32_t ULONG;  // 32 bit long in VS
typedef ULONG *PULONG;
typedef uint16_t USHORT;
typedef unsigned char UCHAR;
typedef char *PSZ;
#endif  /* !BASETYPES */

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

typedef intptr_t INT_PTR;
typedef uintptr_t UINT_PTR;

typedef __int64 LONGLONG;

typedef uint32_t UINT32;
typedef uint8_t UINT8;
typedef int32_t INT32;


#define MAX_PATH          260

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef OPTIONAL
#define OPTIONAL
#endif

#undef far
#undef near
#undef pascal

#define far
#define near
#if (!defined(_MAC)) && ((_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED))
#define pascal __stdcall
#else
#define pascal
#endif

#undef FAR
#undef NEAR
#define FAR                 far
#define NEAR                near
#ifndef CONST
#define CONST               const
#endif

typedef uint32_t            DWORD;
typedef int32_t             BOOL;
typedef unsigned char       BYTE;
typedef uint16_t            WORD;
typedef float               FLOAT;
typedef BOOL near           *PBOOL;
typedef BOOL far            *LPBOOL;
typedef BYTE near           *PBYTE;
typedef BYTE far            *LPBYTE;
typedef WORD near           *PWORD;
typedef WORD far            *LPWORD;
typedef DWORD near          *PDWORD;
typedef DWORD far           *LPDWORD;
typedef void far            *LPVOID;
typedef CONST void far      *LPCVOID;

typedef int32_t             INT;
typedef uint32_t            UINT;

#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l)           ((WORD)(l))
#define HIWORD(l)           ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))

typedef DWORD   COLORREF;
typedef DWORD   *LPCOLORREF;


////////////////////////// WINNT ///////////////////////////////
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef int32_t /*long*/ LONG;
typedef SHORT *PSHORT;
typedef LONG *PLONG;
typedef wchar_t WCHAR;

typedef LONG            HRESULT;
typedef intptr_t        LONG_PTR;
typedef uintptr_t       ULONG_PTR;
typedef LONG_PTR        LRESULT;
typedef ULONG_PTR       DWORD_PTR;

typedef DWORD           LCID,       *PLCID;

typedef unsigned __int64 UINT64, *PUINT64;

//
// ANSI (Multi-byte Character) types
//
typedef CHAR *PCHAR;
typedef CHAR *LPCH, *PCH;

typedef CONST CHAR *LPCCH, *PCCH;
typedef CHAR *NPSTR;
typedef CHAR *LPSTR, *PSTR;
typedef CONST CHAR *LPCSTR, *PCSTR;

typedef LPSTR LPTCH, PTCH;
typedef LPSTR PTSTR, LPTSTR;
typedef LPCSTR LPCTSTR;
typedef LPSTR LP;

typedef WCHAR *LPWSTR;
typedef CONST WCHAR *LPCWSTR;

#ifndef _TCHAR_DEFINED
// TCHAR a typedef or a define?
// a define for the single reason that
// othwerise QTCreator does not show the values (i.e. string) while debugging
// it treats it as an array of bytes
#define TCHAR char
typedef TCHAR _TCHAR, *PTCHAR;
typedef unsigned char TBYTE , *PTBYTE ;
#define _TCHAR_DEFINED
#endif /* !_TCHAR_DEFINED */

// TCHAR support
#define __TEXT(quote) quote         // r_winnt
#define TEXT(quote) __TEXT(quote)   // r_winnt

#define WINAPI
#define __stdcall
#define CALLBACK

#ifdef _DEBUG
#define _ASSERT(expr)   assert(expr)
#else
#define _ASSERT(expr)
#endif

#define __interface struct
#define __forceinline inline

#define _tmain main

typedef void * HWND;
typedef LONG_PTR LPARAM;
typedef UINT_PTR WPARAM;

typedef int errno_t;

#ifdef __cplusplus
}
#endif

#endif /* _WINDEF_ */
