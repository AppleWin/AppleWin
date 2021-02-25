#pragma once

#include <string>

namespace common2
{

  struct EmulatorOptions;

  std::string GetConfigFile(const std::string & filename);
  void InitializeFileRegistry(const EmulatorOptions & options);

}
