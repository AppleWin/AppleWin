#include <cstddef>

namespace ra2
{

  class DiskControl;

  class RetroSerialisation
  {
  public:
    static size_t getSize();
    static void serialise(void * data, size_t size, const DiskControl & diskControl);
    static void deserialise(const void * data, size_t size, DiskControl & diskControl);
  };

}
