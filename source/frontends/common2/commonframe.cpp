#include "StdAfx.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/utils.h"
#include "linux/resources.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "Log.h"
#include "Core.h"
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

  std::string getResourcePath(const std::string & target)
  {
    std::vector<std::string> paths;

    char self[1024] = {0};
    const int ch = readlink("/proc/self/exe", self,  sizeof(self));
    if (ch != -1)
    {
      const char * path = dirname(self);

      // case 1: run from the build folder
      paths.emplace_back(std::string(path) + '/'+ ROOT_PATH);
      // case 2: run from the installation folder
      paths.emplace_back(std::string(path) + '/'+ SHARE_PATH);
    }

    // case 3: use the source folder
    paths.emplace_back(CMAKE_SOURCE_DIR);

    for (const std::string & path : paths)
    {
      char * real = realpath(path.c_str(), nullptr);
      if (real)
      {
        const std::string resourcePath = std::string(real) + target;
        free(real);
        if (dirExists(resourcePath))
        {
          return resourcePath;
        }
      }
    }

    throw std::runtime_error("Cannot found the resource path: " + target);
  }

}

namespace common2
{

  CommonFrame::CommonFrame()
    : myResourcePath(getResourcePath("/resource/"))
  {
    // should this go down to LinuxFrame (maybe Initialisation?)
    g_sProgramDir = getResourcePath("/bin/");
  }

  void CommonFrame::Initialize()
  {
    InitialiseEmulator();
    LinuxFrame::Initialize();
  }

  void CommonFrame::Destroy()
  {
    LinuxFrame::Destroy();
    myResource.clear();
    DestroyEmulator();
  }

  void CommonFrame::Restart()
  {
    Destroy();
    Initialize();
  }

  BYTE* CommonFrame::GetResource(WORD id, LPCSTR lpType, DWORD expectedSize)
  {
    myResource.clear();

    const std::string & filename = getResourceName(id);
    const std::string path = myResourcePath + filename;

    const int fd = open(path.c_str(), O_RDONLY);

    if (fd != -1)
    {
      struct stat stdbuf;
      if ((fstat(fd, &stdbuf) == 0) && S_ISREG(stdbuf.st_mode))
      {
        const off_t size = stdbuf.st_size;
        std::vector<BYTE> data(size);
        const ssize_t rd = read(fd, data.data(), size);
        if (rd == expectedSize)
        {
          std::swap(myResource, data);
        }
      }
      close(fd);
    }

    if (myResource.empty())
    {
      LogFileOutput("FindResource: could not load resource %s\n", filename.c_str());
    }

    return myResource.data();
  }

  std::string CommonFrame::getBitmapFilename(const std::string & resource)
  {
    if (resource == "CHARSET40") return "CHARSET4.BMP";
    if (resource == "CHARSET82") return "CHARSET82.bmp";
    if (resource == "CHARSET8M") return "CHARSET8M.bmp";
    if (resource == "CHARSET8C") return "CHARSET8C.bmp";

    return resource;
  }

}
