#include <cstddef>

namespace ra2
{

  class RetroSerialisation
  {
  public:
    static size_t getSize();
    static bool serialise(void * data, size_t size);
    static bool deserialise(const void * data, size_t size);
  };

}
