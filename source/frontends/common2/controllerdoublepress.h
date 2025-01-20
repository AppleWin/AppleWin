#pragma once

#include <chrono>

namespace common2
{

    class ControllerDoublePress
    {
    public:
        bool pressButton(); // if it returns true, action is triggered
    private:
        // too short and it becomes hard to trigger with SDL.
        static constexpr long myThreshold = 1000;
        std::chrono::steady_clock::time_point myFirstBtnPress;
    };

} // namespace common2