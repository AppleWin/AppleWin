#include "StdAfx.h"
#include "frontends/common2/gnuframe.h"
#include "frontends/common2/fileregistry.h"
#include "apple2roms_data.h"

#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#ifdef __APPLE__
#include "mach-o/dyld.h"
#endif

#include "Core.h"
#include "config.h"

#ifdef __MINGW32__
#define realpath(N, R) _fullpath((R), (N), 0)
#endif

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

#ifdef _WIN32
    int ch = GetModuleFileNameA(0, self, sizeof(self));
    if (ch >= sizeof(self))
    {
        ch = -1;
    }
#elif __APPLE__
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
    }

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

    // this is now used only for g_sProgramDir
    // which only matters for Debug Symbols and Printer Filename
    // let's be tolerant and use cwd

    if (GetCurrentDirectory(sizeof(self), self))
    {
      return std::string(self) + PATH_SEPARATOR;
    }

    throw std::runtime_error("Cannot find the resource path for: " + target);
  }

}

namespace common2
{

  GNUFrame::GNUFrame(const EmulatorOptions & options)
  : CommonFrame(options)
  , myHomeDir(GetHomeDir())
  {
    // should this go down to LinuxFrame (maybe Initialisation?)
    g_sProgramDir = getResourceFolder("/bin/");
    LogFileOutput("Home Dir: '%s'\n", myHomeDir.c_str());
    LogFileOutput("Program Dir: '%s'\n", g_sProgramDir.c_str());
  }

  std::pair<const unsigned char *, unsigned int> GNUFrame::GetResourceData(WORD id) const
  {
    const auto it = apple2roms::data.find(id);
    if (it == apple2roms::data.end())
    {
      throw std::runtime_error("Cannot locate resource: " + std::to_string(id));
    }
    return it->second;
  }

  std::string GNUFrame::Video_GetScreenShotFolder() const
  {
    return myHomeDir + "/Pictures/";
  }

}
