#pragma once

#include <string>

namespace common2
{

  void setSnapshotFilename(const std::string & filename, const bool load);

  class Initialisation
  {
  public:
    Initialisation();
    ~Initialisation();
  };

}
