#pragma once

#include "linux/paddle.h"

#include <vector>

namespace ra2
{

    struct InputDescriptor
    {
        unsigned device;
        unsigned index;
        unsigned id;
    };

    class ControllerBase : public Paddle
    {
    public:
        ControllerBase(const std::vector<InputDescriptor> &buttonCodes);

        bool getButton(int i) const override;

    private:
        const std::vector<InputDescriptor> myButtonCodes;
    };

    class JoypadBase : public ControllerBase
    {
    public:
        JoypadBase();
    };

} // namespace ra2
