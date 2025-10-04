#pragma once

#include "frontends/libretro/input/joypadbase.h"

namespace ra2
{

    class Mouse : public ControllerBase
    {
    public:
        Mouse(const std::unique_ptr<Game> &game, unsigned port);

        double getAxis(int i) const override;
    };

} // namespace ra2
