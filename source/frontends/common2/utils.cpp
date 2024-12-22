#include "StdAfx.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"

#include "SaveState.h"
#include "Registry.h"

#include <libgen.h>
#include <unistd.h>

namespace common2
{

  void setSnapshotFilename(const std::string & filename)
  {
    if (!filename.empty())
    {
      // it is unbelievably hard to convert a path to absolute
      // unless the file exists
      char * temp = strdup(filename.c_str());
      const char * dir = dirname(temp);
      // dir points inside temp!
      SetCurrentDirectory(dir);
      Snapshot_SetFilename(filename);
      free(temp);
      RegSaveString(TEXT(REG_CONFIG), TEXT(REGVALUE_SAVESTATE_FILENAME), 1, Snapshot_GetPathname());
    }
  }

  void loadGeometryFromRegistry(const std::string &section, Geometry & geometry)
  {
    const std::string path = section + "\\geometry";
    const auto loadValue = [&path](const char * name, int & dest)
    {
      uint32_t value;
      if (RegLoadValue(path.c_str(), name, TRUE, &value))
      {
        // uint32_t and int have the same size
        // but if they did not, this would be necessary
        typedef std::make_signed<uint32_t>::type signed_t;
        dest = static_cast<signed_t>(value);
      }
    };

    loadValue("width", geometry.width);
    loadValue("height", geometry.height);
    loadValue("x", geometry.x);
    loadValue("y", geometry.y);
  }

  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry)
  {
    const std::string path = section + "\\geometry";
    const auto saveValue = [&path](const char * name, const int source)
    {
      // this seems to already do the right thing for negative numbers
      RegSaveValue(path.c_str(), name, TRUE, source);
    };
    saveValue("width", geometry.width);
    saveValue("height", geometry.height);
    saveValue("x", geometry.x);
    saveValue("y", geometry.y);
  }

  RestoreCurrentDirectory::RestoreCurrentDirectory()
  {
    char szFilename[MAX_PATH];
    if (GetCurrentDirectory(sizeof(szFilename), szFilename))
    {
      myCurrentDirectory = szFilename;
    }
  }

  RestoreCurrentDirectory::~RestoreCurrentDirectory()
  {
    if (!myCurrentDirectory.empty())
    {
      SetCurrentDirectory(myCurrentDirectory.c_str());
    }
  }

}
