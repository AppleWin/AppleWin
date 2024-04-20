#include <cstdint>
#include <list>

namespace sa2
{

  class ImGuiMemory
  {
  public:
    bool show = false;

    void draw();

  private:
    uint16_t myNewAddress = 0;
    int myBytesPerRow = 16;
    int myNumberOfRows = 32;
    std::list<size_t> myMemoryAddresses;

    void drawSingleView(const size_t address);
  };

}
