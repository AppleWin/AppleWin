#pragma once

#include "frontends/libretro/environment.h"
#include <map>
#include <cstdint>
#include <tuple>

namespace ra2
{

    struct InputDescriptor
    {
        unsigned device;
        unsigned index;
        unsigned id;

        // Comparison operators for use in std::map
        bool operator<(const InputDescriptor &other) const
        {
            return std::tie(device, index, id) < std::tie(other.device, other.index, other.id);
        }
    };

    class InputRemapper
    {
    public:
        InputRemapper(const bool supportsInputBitmasks);

        int16_t inputState(unsigned port, const InputDescriptor &descriptor) const;

        double getMousePosition(unsigned port, int i) const;

        // we can only remap to ASCII characters for the moment
        // only device = RETRO_DEVICE_JOYPAD, index = 0
        void remapButton(unsigned id, char key);

        // when a new controller is added
        void resetPort(unsigned port);

        // this is no-op unless device is a RETRO_DEVICE_JOYPAD or a RETRO_DEVICE_ANALOG
        void processRemappedButtons(unsigned port, unsigned device);

        void mouseEmulation(unsigned port, unsigned device, const double speed);

    private:
        const bool mySupportsInputBitmasks;

        struct MousePosition_t
        {
            InputDescriptor descriptor;
            double multiplier;
            double positions[MAX_PADS] = {}; // -1 to 1 (mutable)
        };

        MousePosition_t myMouse[2];

        size_t myButtonStates[MAX_PADS] = {};
        std::map<InputDescriptor, char> myButtonRemapped;

        size_t getButtonStates(unsigned port);
    };

} // namespace ra2
