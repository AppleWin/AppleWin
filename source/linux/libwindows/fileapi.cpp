#include "fileapi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace
{
  struct FILE_HANDLE : public CHANDLE
  {
    FILE_HANDLE(FILE * ff) : f(ff) {}
    FILE * f = nullptr;
    ~FILE_HANDLE() override
    {
      if (f)
      {
        fclose(f);
      }
    }
  };
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
       PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
  const FILE_HANDLE & file_handle = dynamic_cast<FILE_HANDLE &>(*hFile);

  const int res = fseek(file_handle.f, lDistanceToMove, dwMoveMethod);
  if (res)
  {
    return INVALID_SET_FILE_POINTER;
  }
  else
  {
    return ftell(file_handle.f);
  }
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)   {
  const FILE_HANDLE & file_handle = dynamic_cast<FILE_HANDLE &>(*hFile);

  /* read something from file */
  DWORD bytesread = fread(lpBuffer, 1, nNumberOfBytesToRead, file_handle.f);
  *lpNumberOfBytesRead = bytesread;
  return nNumberOfBytesToRead == bytesread;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {

  const FILE_HANDLE & file_handle = dynamic_cast<FILE_HANDLE &>(*hFile);

  /* write something to file */
  DWORD byteswritten = fwrite(lpBuffer, 1, nNumberOfBytesToWrite, file_handle.f);
  *lpNumberOfBytesWritten = byteswritten;
  return nNumberOfBytesToWrite == byteswritten;
}

BOOL DeleteFile(LPCTSTR lpFileName) {
  if (remove(lpFileName) == 0)
    return TRUE;
  else
    return FALSE;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
  const FILE_HANDLE & file_handle = dynamic_cast<FILE_HANDLE &>(*hFile);
  /* what is the size of the specified file??? Hmmm, really I donna. ^_^ */
  long lcurset = ftell(file_handle.f); // remember current file position

  fseek(file_handle.f, 0, FILE_END);    // go to the end of file
  DWORD lfilesize = ftell(file_handle.f); // that is the real size of file, isn't it??
  fseek(file_handle.f, lcurset, FILE_BEGIN); // let the file position be the same as before
  return lfilesize;
}

HANDLE CreateFile(LPCTSTR               lpFileName,
                  DWORD                 dwDesiredAccess,
                  DWORD                 dwShareMode,
                  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                  DWORD                 dwCreationDisposition,
                  DWORD                 dwFlagsAndAttributes,
                  HANDLE                hTemplateFile)
{
  FILE * f = nullptr;
  if (dwCreationDisposition == CREATE_NEW || dwCreationDisposition == CREATE_ALWAYS)
  {
    if (dwDesiredAccess & GENERIC_READ)
      f = fopen(lpFileName, "w+");
    else
      f = fopen(lpFileName, "w");
  }

  if (dwCreationDisposition == OPEN_EXISTING)
  {
    if (dwDesiredAccess & GENERIC_WRITE)
      f = fopen(lpFileName, "r+");
    else
      f = fopen(lpFileName, "r");
  }

  if (f)
  {
    return new FILE_HANDLE(f);
  }
  else
  {
    return INVALID_HANDLE_VALUE;
  }
}

DWORD GetFileAttributes(const char * filename)
{
  // minimum is R_OK
  // no point checking F_OK as it is useless
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
  // what is the filename does not exist?
  // unfortunately realpath returns -ENOENT
  const char * result = realpath(filename, buffer);
  if (!result)
  {
    *buffer = 0;
  }
  return strlen(buffer);
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

BOOL GetOpenFileName(LPOPENFILENAME lpofn)
{
  return FALSE;
}

BOOL GetSaveFileName(LPOPENFILENAME lpofn)
{
  return FALSE;
}
