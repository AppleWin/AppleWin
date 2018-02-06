#include "linux/version.h"
#include "linux/version.hpp"

#define xstr(a) str(a)
#define str(a, b, c, d) #a"."#b"."#c"."#d

std::string getVersion()
{
  return xstr(FILEVERSION);
}
