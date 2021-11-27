#include "StdAfx.h"
#include "SaveState.h"

#include "frontends/libretro/serialisation.h"
#include "frontends/libretro/environment.h"

#include <cstdio>
#include <fstream>

namespace
{
  class AutoFile
  {
  public:
    AutoFile();
    ~AutoFile();

    std::string getFilename() const;  // only if true

    operator bool() const;
  protected:
    const char * myFilename;
  };

  AutoFile::AutoFile()
  {
    // massive race condition, but without changes to AW, little can we do here
    myFilename = std::tmpnam(nullptr);
  }

  AutoFile::~AutoFile()
  {
    std::remove(myFilename);
  }

  AutoFile::operator bool() const
  {
    return myFilename;
  }

  std::string AutoFile::getFilename() const
  {
    return myFilename;
  }

  void saveToFile(const std::string & filename)  // cannot be null!
  {
    Snapshot_SetFilename(filename);
    Snapshot_SaveState();
  }

}

namespace ra2
{

  size_t RetroSerialisation::getSize()
  {
    AutoFile autoFile;
    if (!autoFile)
    {
      return 0;
    }

    const std::string filename = autoFile.getFilename();
    saveToFile(filename);
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);

    const std::ifstream::pos_type fileSize = ifs.tellg();
    return sizeof(uint32_t) + fileSize;
  }

  bool RetroSerialisation::serialise(char * data, size_t size)
  {
    AutoFile autoFile;
    if (!autoFile)
    {
      return 0;
    }

    const std::string filename = autoFile.getFilename();
    saveToFile(filename);
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);

    const std::ifstream::pos_type fileSize = ifs.tellg();
    if (sizeof(uint32_t) + fileSize > size)
    {
      return false;
    }
    else
    {
      uint32_t * sizePtr = reinterpret_cast<uint32_t *>(data);
      *sizePtr = fileSize;

      char * dataPtr = data + sizeof(uint32_t);

      ifs.seekg(0, std::ios::beg);
      ifs.read(dataPtr, fileSize);

      return true;
    }
  }

  bool RetroSerialisation::deserialise(const char * data, size_t size)
  {
    AutoFile autoFile;
    if (!autoFile)
    {
      return false;
    }

    const uint32_t * sizePtr = reinterpret_cast<const uint32_t *>(data);
    const size_t fileSize = *sizePtr;

    if (sizeof(uint32_t) + fileSize > size)
    {
      return false;
    }
    else
    {
      const char * dataPtr = data + sizeof(uint32_t);

      const std::string filename = autoFile.getFilename();
      // do not remove the {} scope below!
      {
        std::ofstream ofs(filename, std::ios::binary);
        ofs.write(dataPtr, fileSize);
      }

      Snapshot_SetFilename(filename);
      Snapshot_LoadState();
      return true;
    }
  }

}
