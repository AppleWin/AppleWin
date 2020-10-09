#include "StdAfx.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "Log.h"
#include "config.h"

namespace
{

  bool dirExists(const std::string & folder)
  {
    struct stat stdbuf;

    if (stat(folder.c_str(), &stdbuf) == 0 && S_ISDIR(stdbuf.st_mode))
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  std::string getResourcePathImpl()
  {
    char self[1024] = {0};
    const int ch = readlink("/proc/self/exe", self,  sizeof(self));
    if (ch != -1)
    {
      const char * path = dirname(self);

      // case 1: run from the build folder
      const std::string path1 = std::string(path) + '/'+ ROOT_PATH + "/resource/";
      if (dirExists(path1))
      {
	return path1;
      }

      // case 2: run from the installation folder
      const std::string path2 = std::string(path) + '/'+ SHARE_PATH + "/resource/";
      if (dirExists(path2))
      {
	return path2;
      }
    }
    // else?
    return std::string();
  }

}

const std::string & getResourcePath()
{
  static const std::string path = getResourcePathImpl();
  return path;
}

HRSRC FindResource(void *, const char * filename, const char *)
{
  HRSRC result;

  if (filename)
  {
    const std::string path = getResourcePath() + filename;

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
