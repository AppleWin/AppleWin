#pragma once

#include <memory>

namespace na2
{

    class NFrame;

    int GetKeyPressed(const std::shared_ptr<NFrame> &frame);
    void SetCtrlCHandler(const bool headless);

    extern double g_relativeSpeed;

    extern bool g_stop;

} // namespace na2
