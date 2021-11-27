#include <cstddef>

namespace ra2
{

  class RetroSerialisation
  {
  public:
    static size_t getSize();
    static bool serialise(char * data, size_t size);
    static bool deserialise(const char * data, size_t size);
  };

}
