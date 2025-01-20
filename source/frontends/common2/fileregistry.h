#pragma once

#include <memory>

class Registry;

namespace common2
{

    struct EmulatorOptions;

    std::shared_ptr<Registry> CreateFileRegistry(const EmulatorOptions &options);

} // namespace common2
