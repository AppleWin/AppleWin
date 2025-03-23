#pragma once

namespace na2
{

    class NFrame;

    int GetKeyPressed(NFrame &frame);
    void SetCtrlCHandler(const bool headless);

    extern double g_relativeSpeed;

    extern bool g_stop;

} // namespace na2
