#include "frontends/libretro/buffer.h"
#include <vector>
#include <string>
#include <filesystem>

namespace ra2
{

  class DiskControl
  {
  public:
    DiskControl();

    bool getEjectedState() const;
    bool setEjectedState(bool state);

    size_t getImageIndex() const;
    bool setImageIndex(size_t index);

    size_t getNumImages() const;

    bool replaceImageIndex(size_t index, const std::string & path);
    bool removeImageIndex(size_t index);
    bool addImageIndex();

    // these 2 functions update the images for the Disc Control Interface
    bool insertDisk(const std::string & filename);
    bool insertPlaylist(const std::string & filename);

    bool getImagePath(unsigned index, char *path, size_t len) const;
    bool getImageLabel(unsigned index, char *label, size_t len) const;

    static void setInitialPath(unsigned index, const char *path);

    void serialise(Buffer<char> & buffer) const;
    void deserialise(Buffer<char const> & buffer);

  private:
    std::vector<std::filesystem::path> myImages;

    bool myEjected;
    size_t myIndex;

    bool insertFloppyDisk(const std::string & filename) const;
    bool insertHardDisk(const std::string & filename) const;

    static unsigned ourInitialIndex;
    static std::string ourInitialPath;
  };

}
