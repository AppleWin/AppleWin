#pragma once

#include "frontends/libretro/input/joypadbase.h"

#include <vector>

namespace ra2
{

    class Analog : public JoypadBase
    {
    public:
        Analog(const std::unique_ptr<Game> &game, unsigned port);

        double getAxis(int i) const override;

    private:
        std::vector<InputDescriptor> myAxisCodes;
    };

} // namespace ra2
