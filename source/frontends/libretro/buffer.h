#include <cstddef>
#include <stdexcept>

namespace ra2
{

  template<typename C> // so it works with both char * and const char *
  class Buffer
  {
  public:
    Buffer(C * const begin, size_t const size) : myPtr(begin), myEnd(begin + size)
    {

    }

    template <typename T>
    T & get()
    {
      C * c = myPtr;
      advance(sizeof(T));
      return *reinterpret_cast<T *>(c);
    }

    void get(size_t const size, C * & begin, C * & end)
    {
      begin = myPtr;
      advance(size);
      end = myPtr;
    }

  private:
    C * myPtr;
    C * const myEnd;

    void advance(size_t const size)
    {
      if (myPtr + size > myEnd)
      {
        throw std::runtime_error("Buffer: out of bounds");
      }
      myPtr += size;
    }
  };

}
