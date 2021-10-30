#pragma once

#include <string>

namespace common2
{
  struct Geometry;

  void setSnapshotFilename(const std::string & filename, const bool load);

  // Do not call directly. Used in CommonFrame
  void InitialiseEmulator();
  void DestroyEmulator();

  void loadGeometryFromRegistry(const std::string &section, Geometry & geometry);
  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry);
}
