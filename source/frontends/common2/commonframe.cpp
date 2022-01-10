#include "StdAfx.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/utils.h"
#include "linux/resources.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "Log.h"
#include "SaveState.h"

namespace common2
{

  void CommonFrame::LoadSnapshot()
  {
    Snapshot_LoadState();
  }

  BYTE* CommonFrame::GetResource(WORD id, LPCSTR lpType, DWORD expectedSize)
  {
    myResource.clear();

    const std::string & filename = getResourceName(id);
    const std::string path = getResourcePath(filename);

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
