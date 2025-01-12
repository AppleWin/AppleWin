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

    char szFilename[MAX_PATH];
    if (GetCurrentDirectory(sizeof(szFilename), szFilename))
    {
      return std::string(szFilename) + PATH_SEPARATOR;
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
