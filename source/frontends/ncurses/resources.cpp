#include "StdAfx.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "Log.h"

HRSRC FindResource(void *, const char * filename, const char *)
{
  HRSRC result;

  if (filename)
  {
    const std::string path = std::string("resource/") + filename;

    int fd = open(path.c_str(), O_RDONLY);

    if (fd != -1)
    {
      struct stat stdbuf;
      if ((fstat(fd, &stdbuf) == 0) && S_ISREG(stdbuf.st_mode))
      {
	const off_t size = stdbuf.st_size;
	result.data.resize(size);
	ssize_t rd = read(fd, result.data.data(), size);
      }
      close(fd);
    }
  }

  if (result.data.empty())
  {
    LogFileOutput("FindResource: could not load resource %s\n", filename);
  }

  return result;
}

HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename)
{
  LogFileOutput("LoadBitmap: not loading resource %s\n", filename);
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  memset(lpvBits, 0, cb);
  return cb;
}
