#pragma once

#include <map>
#include <string>

namespace common2
{

    // Write nested map to YAML file
    void writeMapToYaml(
        const std::string &filename, const std::map<std::string, std::map<std::string, std::string>> &data);

    // Read nested map from YAML file
    std::map<std::string, std::map<std::string, std::string>> readMapFromYaml(const std::string &filename);

} // namespace common2
