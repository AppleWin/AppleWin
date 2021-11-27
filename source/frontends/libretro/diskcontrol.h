#include <vector>
#include <string>

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

    bool insertDisk(const std::string & filename);

    bool getImagePath(unsigned index, char *path, size_t len) const;
    bool getImageLabel(unsigned index, char *label, size_t len) const;

  private:
    std::vector<std::string> myImages;

    bool myEjected;
    size_t myIndex;

    bool insertFloppyDisk(const std::string & filename) const;
    bool insertHardDisk(const std::string & filename) const;

    void checkState() const;
  };

}
