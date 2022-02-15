#include "StdAfx.h"
#include "frontends/common2/gnuframe.h"
#include "frontends/common2/fileregistry.h"

#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#ifdef __APPLE__
#include "mach-o/dyld.h"
#endif

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

  std::string getResourceFolder(const std::string & target)
  {
    std::vector<std::string> paths;

    char self[1024] = {0};

#ifdef __APPLE__
    uint32_t size = sizeof(self);
    const int ch = _NSGetExecutablePath(self, &size);
#else
    const int ch = readlink("/proc/self/exe", self,  sizeof(self));
#endif

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

  GNUFrame::GNUFrame()
  : myHomeDir(GetHomeDir())
  , myResourceFolder(getResourceFolder("/resource/"))
  {
    // should this go down to LinuxFrame (maybe Initialisation?)
    g_sProgramDir = getResourceFolder("/bin/");
  }

  std::string GNUFrame::getResourcePath(const std::string & filename)
  {
    return myResourceFolder + filename;
  }

  std::string GNUFrame::Video_GetScreenShotFolder() const
  {
    return myHomeDir + "/Pictures/";
  }

}
