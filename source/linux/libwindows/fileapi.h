#pragma once

#include "wincompat.h"
#include "winhandles.h"

#define INVALID_FILE_ATTRIBUTES (~0u)
#define INVALID_SET_FILE_POINTER (~0u)

typedef struct tagOFN
{
    DWORD lStructSize;
    HWND hwndOwner;
    HINSTANCE hInstance;
    LPCTSTR lpstrFilter;
    LPTSTR lpstrFile;
    DWORD nMaxFile;
    LPCTSTR lpstrInitialDir;
    LPCTSTR lpstrTitle;
    DWORD Flags;
    WORD nFileExtension;
    WORD nFileOffset;
} OPENFILENAME, *LPOPENFILENAME;

#define FILE_BEGIN SEEK_SET
#define FILE_CURRENT SEEK_CUR
#define FILE_END SEEK_END

#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_NORMAL 0

#define OFN_READONLY 0x00000001
#define OFN_HIDEREADONLY 0x00000004
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OPEN_EXISTING 3
#define CREATE_NEW 1
#define CREATE_ALWAYS 2

#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 0x00000001

#define _MAX_FNAME 256
#define _MAX_EXT _MAX_FNAME

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);

BOOL ReadFile(
    HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

BOOL WriteFile(
    HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten,
    LPOVERLAPPED lpOverlapped);

BOOL DeleteFile(LPCTSTR lpFileName);

DWORD GetFileAttributes(const char *filename);
DWORD GetFullPathName(const char *filename, DWORD, char *, char **);
BOOL GetOpenFileName(LPOPENFILENAME lpofn);
BOOL GetSaveFileName(LPOPENFILENAME lpofn);

HANDLE CreateFile(
    LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);

// non "/" terminated
DWORD GetCurrentDirectory(DWORD, char *);
