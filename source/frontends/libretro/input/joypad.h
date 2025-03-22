#pragma once

#include "frontends/libretro/input/joypadbase.h"

#include <vector>

namespace ra2
{

    class Joypad : public JoypadBase
    {
    public:
        Joypad(const std::unique_ptr<Game> &game, unsigned port);

        double getAxis(int i) const override;

    private:
        std::vector<std::vector<std::pair<InputDescriptor, double>>> myAxisCodes;
    };

} // namespace ra2
