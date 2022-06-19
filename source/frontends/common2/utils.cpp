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
      chdir(dir);
      Snapshot_SetFilename(filename);

      free(temp);
    }
  }

  void loadGeometryFromRegistry(const std::string &section, std::optional<Geometry> & geometry)
  {
    if (!geometry)  // otherwise it was user provided
    {
      const std::string path = section + "\\geometry";
      const auto loadValue = [&path](const char * name, int & dest)
      {
        DWORD value;
        if (RegLoadValue(path.c_str(), name, TRUE, &value))
        {
          // DWORD and int have the same size
          // but if they did not, this would be necessary
          typedef std::make_signed<DWORD>::type signed_t;
          dest = static_cast<signed_t>(value);
        }
      };

      geometry = Geometry();
      loadValue("width", geometry->width);
      loadValue("height", geometry->height);
      loadValue("x", geometry->x);
      loadValue("y", geometry->y);
    }
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

}
