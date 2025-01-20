#pragma once

#include "frontends/libretro/rkeyboard.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/diskcontrol.h"

#include <memory>

class Registry;

namespace common2
{
    class PTreeRegistry;
}

namespace ra2
{

    void SetupRetroVariables();
    std::shared_ptr<common2::PTreeRegistry> CreateRetroRegistry();
    void PopulateRegistry(const std::shared_ptr<Registry> &registry);

    AudioSource GetAudioSource();
    KeyboardType GetKeyboardEmulationType();
    PlaylistStartDisk GetPlaylistStartDisk();

} // namespace ra2
