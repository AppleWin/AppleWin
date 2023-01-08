#include "StdAfx.h"
#include "SaveState.h"

#include "frontends/libretro/serialisation.h"
#include "frontends/libretro/diskcontrol.h"

#include <cstdio>
#include <fstream>

namespace
{
  class AutoFile
  {
  public:
    AutoFile();
    ~AutoFile();

    const std::string & getFilename() const;  // only if true

  protected:
    std::string myFilename;
  };

  AutoFile::AutoFile()
  {
    // massive race condition, but without changes to AW, little can we do here
    const char * tmp = std::tmpnam(nullptr);
    if (!tmp)
    {
      throw std::runtime_error("Cannot create temporary file");
    }
    myFilename = tmp;
  }

  AutoFile::~AutoFile()
  {
    std::remove(myFilename.c_str());
  }

  const std::string & AutoFile::getFilename() const
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
    std::string const & filename = autoFile.getFilename();
    saveToFile(filename);
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);

    const size_t fileSize = ifs.tellg();
    // we add a buffer to include a few things
    // DiscControl images
    // various sizes
    // small variations in AW yaml format
    const size_t buffer = 4096;
    return fileSize + buffer;
  }

  void RetroSerialisation::serialise(void * data, size_t size, const DiskControl & diskControl)
  {
    Buffer buffer(reinterpret_cast<char *>(data), size);
    diskControl.serialise(buffer);

    AutoFile autoFile;
    std::string const & filename = autoFile.getFilename();
    saveToFile(filename);
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);

    size_t const fileSize = ifs.tellg();
    buffer.get<size_t>() = fileSize;

    char * begin, * end;
    buffer.get(fileSize, begin, end);

    ifs.seekg(0, std::ios::beg);
    ifs.read(begin, end - begin);
  }

  void RetroSerialisation::deserialise(const void * data, size_t size, DiskControl & diskControl)
  {
    Buffer buffer(reinterpret_cast<const char *>(data), size);
    diskControl.deserialise(buffer);

    const size_t fileSize = buffer.get<size_t const>();

    AutoFile autoFile;
    std::string const & filename = autoFile.getFilename();
    // do not remove the {} scope below! it ensures the file is flushed
    {
      char const * begin, * end;
      buffer.get(fileSize, begin, end);
      std::ofstream ofs(filename, std::ios::binary);
      ofs.write(begin, end - begin);
    }

    Snapshot_SetFilename(filename);
    Snapshot_LoadState();
  }

}
