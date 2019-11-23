#include "StdAfx.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "Log.h"

HRSRC FindResource(void *, const std::string & filename, const char *)
{
  HRSRC result;

  if (!filename.empty())
  {
    const std::string path = "resource/" + filename;

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
    LogFileOutput("FindResource: could not load resource %s\n", filename.c_str());
  }

  return result;
}

HBITMAP LoadBitmap(HINSTANCE hInstance, const std::string & filename)
{
  LogFileOutput("LoadBitmap: not loading resource %s\n", filename.c_str());
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  memset(lpvBits, 0, cb);
  return cb;
}
