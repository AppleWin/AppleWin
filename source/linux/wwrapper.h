#pragma once

#include "linux/wincompat.h"
#include "linux/dummies.h"

#include <string>
#include <string.h>

#define FILE_BEGIN 	SEEK_SET
#define FILE_CURRENT	SEEK_CUR
#define FILE_END	SEEK_END
#define INVALID_HANDLE_VALUE NULL

#define MEM_COMMIT	0x1000
#define MEM_RELEASE	0
#define MEM_RESERVE     0
#define PAGE_NOACCESS   0
#define PAGE_READWRITE	0

#define OFN_FILEMUSTEXIST 0
#define OFN_PATHMUSTEXIST 0
#define OFN_READONLY 0
#define OFN_HIDEREADONLY 0

#define MB_ICONSTOP 0
#define MB_SETFOREGROUND 0
#define MB_ICONWARNING 0
#define MB_ICONEXCLAMATION 0

#define MB_OK 0
#define MB_YESNO 4
#define MB_YESNOCANCEL 3

#define IDOK 1
#define IDCANCEL 2
#define IDNO 7

#define _MAX_EXT MAX_PATH

#define _ASSERT	assert
#define WINAPI

#define INVALID_SOCKET NULL

#define INVALID_FILE_ATTRIBUTES (DWORD(-1))

#define FILE_ATTRIBUTE_READONLY 0x00000001
#define FILE_ATTRIBUTE_NORMAL 0

#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define FILE_SHARE_READ 0x00000001
#define OPEN_EXISTING 3
#define CREATE_NEW 1

#define _T(x) x

#define MoveMemory(Destination,Source,Length) memmove((Destination),(Source),(Length))
#define FillMemory(Destination,Length,Fill) memset((Destination),(Fill),(Length))
#define EqualMemory(Destination,Source,Length) (!memcmp((Destination),(Source),(Length)))
#define CopyMemory(Destination,Source,Length) memcpy((Destination),(Source),(Length))
#define ZeroMemory(Destination,Length) memset((Destination),0,(Length))

void _tzset();
errno_t ctime_s(char * buf, size_t size, const time_t *time);
void strcpy_s(char * dest, size_t size, const char * source);

DWORD SetFilePointer(HANDLE hFile,
       LONG lDistanceToMove,
       PLONG lpDistanceToMoveHigh,
       DWORD dwMoveMethod);

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
		     LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

 /* close handle whatever it has been .... hmmmmm. I just love Microsoft! */
BOOL CloseHandle(HANDLE hObject);

BOOL DeleteFile(LPCTSTR lpFileName);

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);

LPVOID VirtualAlloc(LPVOID lpAddress, size_t dwSize,
		DWORD flAllocationType, DWORD flProtect);

BOOL VirtualFree(LPVOID lpAddress, size_t dwSize, DWORD dwFreeType);


static inline bool IsCharLower(char ch) {
	return isascii(ch) && islower(ch);
}

static inline bool IsCharUpper(char ch) {
	return isascii(ch) && isupper(ch);
}

DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength);

HANDLE CreateFile(LPCTSTR               lpFileName,
		  DWORD                 dwDesiredAccess,
		  DWORD                 dwShareMode,
		  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		  DWORD                 dwCreationDisposition,
		  DWORD                 dwFlagsAndAttributes,
		  HANDLE                hTemplateFile
);

DWORD GetFileAttributes(const char * filename);
DWORD GetFullPathName(const char* filename, DWORD, char *, char **);

std::string MAKEINTRESOURCE(int x);
HRSRC FindResource(void *, const std::string & filename, const char *);
DWORD SizeofResource(void *, const HRSRC &);
HGLOBAL LoadResource(void *, const HRSRC &);
BYTE * LockResource(HGLOBAL);

DWORD timeGetTime();
DWORD GetCurrentDirectory(DWORD, char *);

UINT64 _strtoui64(const char *, void *, int);

void GetLocalTime(SYSTEMTIME *t);

BOOL GetOpenFileName(LPOPENFILENAME lpofn);
