#pragma once

#include <memory>

class Registry;

namespace ra2
{

  void SetupRetroVariables();
  std::shared_ptr<Registry> CreateRetroRegistry();

}
