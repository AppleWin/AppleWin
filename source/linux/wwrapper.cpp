/*
 * Wrappers for some common Microsoft file and memory functions used in AppleWin
 *	by beom beotiger, Nov 2007AD
*/

#include "linux/wwrapper.h"

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <boost/lexical_cast.hpp>
#include <iomanip>
#include <ctime>

#include "../resource/resource.h"
#include "Log.h"

DWORD SetFilePointer(HANDLE hFile,
       LONG lDistanceToMove,
       PLONG lpDistanceToMoveHigh,
       DWORD dwMoveMethod)	{
	       /* ummm,fseek in Russian */
	       fseek((FILE*)hFile, lDistanceToMove, dwMoveMethod);
	       return ftell((FILE*)hFile);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
       		LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)	{

	/* read something from file */
	DWORD bytesread = fread(lpBuffer, 1, nNumberOfBytesToRead, (FILE*)hFile);
	*lpNumberOfBytesRead = bytesread;
	return (nNumberOfBytesToRead == bytesread);
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
		LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
	/* write something to file */
	DWORD byteswritten = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, (FILE*)hFile);
	*lpNumberOfBytesWritten = byteswritten;
	return (nNumberOfBytesToWrite == byteswritten);
}

/* close handle whatever it has been .... hmmmmm. I just love Microsoft! */
BOOL CloseHandle(HANDLE hObject) {
	return(!fclose((FILE*) hObject));
}

BOOL DeleteFile(LPCTSTR lpFileName) {
	if(remove(lpFileName) == 0) return TRUE;
	else return FALSE;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
	/* what is the size of the specified file??? Hmmm, really I donna. ^_^ */
	long lcurset = ftell((FILE*)hFile); // remember current file position

	fseek((FILE*)hFile, 0, FILE_END);	// go to the end of file
	DWORD lfilesize = ftell((FILE*)hFile); // that is the real size of file, isn't it??
	fseek((FILE*)hFile, lcurset, FILE_BEGIN); // let the file position be the same as before
	return lfilesize;
}

LPVOID VirtualAlloc(LPVOID lpAddress, size_t dwSize,
      DWORD flAllocationType, DWORD flProtect) {
	/* just malloc and alles? 0_0 */
	void* mymemory;
	mymemory = realloc(lpAddress, dwSize);
	if(flAllocationType & MEM_COMMIT) ZeroMemory(mymemory, dwSize); // original VirtualAlloc does this (if..)
	return mymemory;
}

BOOL VirtualFree(LPVOID lpAddress, size_t dwSize,
			DWORD dwFreeType) {
	free(lpAddress);
	return TRUE;
}

// make all chars in buffer lowercase
DWORD CharLowerBuff(LPTSTR lpsz, DWORD cchLength)
{
//		char *s;
	if (lpsz)
		for (lpsz; *lpsz; lpsz++)
			*lpsz = tolower(*lpsz);
	return 1;

}

void _tzset()
{
  tzset();
}

errno_t ctime_s(char * buf, size_t size, const time_t *time)
{
  const char * t = asctime(localtime(time));
  strncpy(buf, t, size);
  return 0;
}

void strcpy_s(char * dest, size_t size, const char * source)
{
  strncpy(dest, source, size);
}

HANDLE CreateFile(LPCTSTR               lpFileName,
		  DWORD                 dwDesiredAccess,
		  DWORD                 dwShareMode,
		  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		  DWORD                 dwCreationDisposition,
		  DWORD                 dwFlagsAndAttributes,
		  HANDLE                hTemplateFile)
{
  if (dwCreationDisposition == CREATE_NEW)
  {
    if (dwDesiredAccess & GENERIC_READ)
      return fopen(lpFileName, "w+");
    else
      return fopen(lpFileName, "w");
  }

  if (dwCreationDisposition == OPEN_EXISTING)
  {
    if (dwDesiredAccess & GENERIC_WRITE)
      return fopen(lpFileName, "r+");
    else
      return fopen(lpFileName, "r");
  }
  return NULL;
}

DWORD GetFileAttributes(const char * filename)
{
  const int exists = access(filename, F_OK);
  if (exists)
    return INVALID_FILE_ATTRIBUTES;

  const int read = access(filename, R_OK);
  if (read)
    return INVALID_FILE_ATTRIBUTES;

  const int write = access(filename, W_OK);
  if (write)
    return FILE_ATTRIBUTE_READONLY;

  return FILE_ATTRIBUTE_NORMAL;
}

DWORD GetFullPathName(const char* filename, DWORD length, char * buffer, char ** filePart)
{
  const char * result = realpath(filename, buffer);
  if (!result)
  {
    *buffer = 0;
  }
  return strlen(buffer);
}

std::string MAKEINTRESOURCE(int x)
{
  switch (x)
  {
  case IDR_APPLE2_ROM: return "Apple2.rom";
  case IDR_APPLE2_PLUS_ROM: return "Apple2_Plus.rom";
  case IDR_APPLE2E_ROM: return "Apple2e.rom";
  case IDR_APPLE2E_ENHANCED_ROM: return "Apple2e_Enhanced.rom";
  case IDR_PRAVETS_82_ROM: return "PRAVETS82.ROM";
  case IDR_PRAVETS_8C_ROM: return "PRAVETS8C.ROM";
  case IDR_PRAVETS_8M_ROM: return "PRAVETS8M.ROM";
  case IDR_TK3000_2E_ROM: return "TK3000e.rom";

  case IDR_DISK2_FW: return "DISK2.rom";
  case IDR_SSC_FW: return "SSC.rom";
  case IDR_HDDRVR_FW: return "Hddrvr.bin";
  case IDR_PRINTDRVR_FW: return "Parallel.rom";
  case IDR_MOCKINGBOARD_D_FW: return "Mockingboard-D.rom";
  case IDR_MOUSEINTERFACE_FW: return "MouseInterface.rom";
  case IDR_THUNDERCLOCKPLUS_FW: return "ThunderClockPlus.rom";
  case IDR_TKCLOCK_FW: return"TKClock.rom";
  }

  LogFileOutput("Unknown resource %d\n", x);
  return std::string();
}

DWORD SizeofResource(void *, const HRSRC & res)
{
  return res.data.size();
}

HGLOBAL LoadResource(void *, const HRSRC & res)
{
  return res.data.data();
}

BYTE * LockResource(HGLOBAL data)
{
  return (BYTE *)data;
}

DWORD timeGetTime()
{
  struct timeval now;
  gettimeofday(&now, NULL);
  return now.tv_usec / 1000;
}

DWORD GetCurrentDirectory(DWORD length, char * buffer)
{
  const char * cwd = getcwd(buffer, length);

  if (cwd)
  {
    return strlen(buffer);
  }
  else
  {
    return 0;
  }
}

void GetLocalTime(SYSTEMTIME *t)
{
  timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  time_t tim = ts.tv_sec;

  t->wMilliseconds = ts.tv_nsec / 1000000;

  tm * local = localtime(&tim);

  t->wSecond = local->tm_sec;
  t->wMinute = local->tm_min;
  t->wHour = local->tm_hour;
  t->wDayOfWeek = local->tm_wday;
  t->wDay = local->tm_mday;
  t->wMonth = local->tm_mon + 1;
  t->wYear = local->tm_year;
}

BOOL GetOpenFileName(LPOPENFILENAME lpofn)
{
  return FALSE;
}

BOOL WINAPI PostMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
  return TRUE;
}

int GetDateFormat(LCID Locale, DWORD dwFlags, CONST SYSTEMTIME *lpDate, LPCSTR lpFormat, LPSTR lpDateStr, int cchDate)
{
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%D");
  const std::string str = ss.str();
  strncpy(lpDateStr, str.c_str(), cchDate);
  return cchDate; // not 100% sure, but it is never used
}

int GetTimeFormat(LCID Locale, DWORD dwFlags, CONST SYSTEMTIME *lpTime, LPCSTR lpFormat, LPSTR lpTimeStr, int cchTime)
{
  std::time_t t = std::time(nullptr);
  std::tm tm = *std::localtime(&t);
  std::ostringstream ss;
  ss << std::put_time(&tm, "%T");
  const std::string str = ss.str();
  strncpy(lpTimeStr, str.c_str(), cchTime);
  return cchTime; // not 100% sure, but it is never used
}
