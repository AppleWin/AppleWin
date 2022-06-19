#pragma once

#include <string>
#include <optional>

namespace common2
{
  struct Geometry;

  void setSnapshotFilename(const std::string & filename);

  void loadGeometryFromRegistry(const std::string &section, std::optional<Geometry> & geometry);
  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry);
}
