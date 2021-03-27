#pragma once

#include <string>

namespace common2
{

  void setSnapshotFilename(const std::string & filename, const bool load);

  // Do not call directly. Used in CommonFrame
  void InitialiseEmulator();
  void DestroyEmulator();

}
