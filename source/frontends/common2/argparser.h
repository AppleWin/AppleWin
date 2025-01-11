#include <string>

namespace common2
{
  struct EmulatorOptions;

  enum class OptionsType { none, applen, sa2 };

  bool getEmulatorOptions(int argc, char *const argv[], OptionsType type, const std::string & edition, EmulatorOptions & options);
}
