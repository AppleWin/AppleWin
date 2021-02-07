#pragma once

#include <string>

struct EmulatorOptions;

std::string GetConfigFile(const std::string & filename);
void InitializeFileRegistry(const EmulatorOptions & options);
