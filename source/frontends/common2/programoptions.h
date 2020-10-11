#include <string>


struct EmulatorOptions
{
  std::string disk1;
  std::string disk2;
  bool createMissingDisks;

  std::string snapshot;

  int memclear;

  bool log;

  bool benchmark;
  bool headless;
  bool ntsc;

  bool saveConfigurationOnExit;
  bool useQtIni;  // use Qt .ini file (read only)

  bool run;  // false if options include "-h"
};

bool getEmulatorOptions(int argc, const char * argv [], const std::string & version, EmulatorOptions & options);
