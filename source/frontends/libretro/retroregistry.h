#pragma once

#include "frontends/libretro/input/rkeyboard.h"
#include "frontends/libretro/diskcontrol.h"

#include <memory>

class Registry;

namespace common2
{
    class PTreeRegistry;
}

namespace ra2
{

    class Game;

    std::shared_ptr<common2::PTreeRegistry> createRetroRegistry();
    void setupRetroVariables();
    void applyRetroVariables(Game &game);

    KeyboardType getKeyboardEmulationType();
    PlaylistStartDisk getPlaylistStartDisk();
    double getMouseSpeed();

} // namespace ra2
