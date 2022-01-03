#pragma once

#include <string>
#include <memory>

class Registry;

namespace common2
{

  struct EmulatorOptions;

  std::string GetConfigFile(const std::string & filename);
  std::shared_ptr<Registry> CreateFileRegistry(const EmulatorOptions & options);
  std::string GetHomeDir();

}
