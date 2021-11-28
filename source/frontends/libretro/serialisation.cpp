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

  struct SerialisationFormat_t
  {
    uint32_t size;
    char data[];  // zero-length array, containing the AW's yaml format
  };

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

  bool RetroSerialisation::serialise(void * data, size_t size)
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
      SerialisationFormat_t * serialised = reinterpret_cast<SerialisationFormat_t *>(data);
      serialised->size = fileSize;

      ifs.seekg(0, std::ios::beg);
      ifs.read(serialised->data, serialised->size);

      return true;
    }
  }

  bool RetroSerialisation::deserialise(const void * data, size_t size)
  {
    AutoFile autoFile;
    if (!autoFile)
    {
      return false;
    }

    const SerialisationFormat_t * serialised = reinterpret_cast<const SerialisationFormat_t *>(data);

    if (sizeof(uint32_t) + serialised->size > size)
    {
      return false;
    }
    else
    {
      const std::string filename = autoFile.getFilename();
      // do not remove the {} scope below!
      {
        std::ofstream ofs(filename, std::ios::binary);
        ofs.write(serialised->data, serialised->size);
      }

      Snapshot_SetFilename(filename);
      Snapshot_LoadState();
      return true;
    }
  }

}
