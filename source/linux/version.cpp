#include "linux/version.h"
#include "../resource/version.h"

#define xstr2(a) str2(a)
#define str2(a, b, c, d) #a"."#b"."#c"."#d

std::string getVersion()
{
  return xstr2(APPLEWIN_VERSION);
}
