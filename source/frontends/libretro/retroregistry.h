#pragma once

#include <memory>

class Registry;

namespace common2
{
  class PTreeRegistry;
}

namespace ra2
{

  void SetupRetroVariables();
  std::shared_ptr<common2::PTreeRegistry> CreateRetroRegistry();
  void PopulateRegistry(const std::shared_ptr<Registry> & registry);

  size_t GetAudioOutputChannels();

}
