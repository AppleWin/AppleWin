#pragma once

#include <string>
#include <filesystem>

namespace common2
{
  struct Geometry;

  void setSnapshotFilename(const std::filesystem::path & filename);

  void loadGeometryFromRegistry(const std::string &section, Geometry & geometry);
  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry);

  std::filesystem::path getSettingsRootDir();
  std::filesystem::path getHomeDir();
  std::filesystem::path getConfigFile(const std::string & filename);

  // partial workaround to the (mis-)usage of chdir()
  class RestoreCurrentDirectory
  {
  public:
    RestoreCurrentDirectory();
    ~RestoreCurrentDirectory();
  private:
    const std::filesystem::path myCurrentDirectory;
  };

}
