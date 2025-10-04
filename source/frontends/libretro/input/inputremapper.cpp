#include "StdAfx.h"
#include "Interface.h"

#include "frontends/libretro/input/inputremapper.h"
#include "frontends/libretro/environment.h"

#include "linux/keyboardbuffer.h"

namespace ra2
{

    InputRemapper::InputRemapper(const bool supportsInputBitmasks)
        : mySupportsInputBitmasks(supportsInputBitmasks)
    {
        Video &video = GetVideo();

        myMouse[0] = {{RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X}, 1.0 / video.GetFrameBufferBorderlessWidth()};
        myMouse[1] = {{RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y}, 1.0 / video.GetFrameBufferBorderlessHeight()};
    }

    int16_t InputRemapper::inputState(unsigned port, const InputDescriptor &descriptor) const
    {
        const auto it = myButtonRemapped.find(descriptor);

        // if it has been remapped, return 0 (i.e. not pressed)
        if (it != myButtonRemapped.end())
        {
            return 0;
        }

        // otherwise, return the original value
        const int16_t value = input_state_cb(port, descriptor.device, descriptor.index, descriptor.id);

        return value;
    }

    double InputRemapper::getMousePosition(unsigned port, int i) const
    {
        return myMouse[i].positions[port];
    }

    void InputRemapper::remapButton(unsigned id, char key)
    {
        // only remap RETRO_DEVICE_JOYPAD, index 0
        const InputDescriptor descriptor{RETRO_DEVICE_JOYPAD, 0, id};
        if (key)
        {
            myButtonRemapped[descriptor] = key;
        }
        else
        {
            myButtonRemapped.erase(descriptor);
        }
    }

    size_t InputRemapper::getButtonStates(unsigned port)
    {
        size_t newState;
        if (mySupportsInputBitmasks)
        {
            newState = input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
        }
        else
        {
            newState = 0;
            for (size_t i = 0; i < RETRO_DEVICE_ID_JOYPAD_R3 + 1; ++i)
            {
                if (input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, i))
                {
                    newState |= 1 << i;
                }
            }
        }
        // if it is active NOW and was NOT before.
        const size_t result = (~myButtonStates[port]) & newState;

        myButtonStates[port] = newState;
        return result;
    }

    void InputRemapper::resetPort(unsigned port)
    {
        myButtonStates[port] = 0;
        for (auto &mouse : myMouse)
        {
            mouse.positions[port] = 0.0;
        }
    }

    void InputRemapper::processRemappedButtons(unsigned port, unsigned device)
    {
        if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_ANALOG)
        {
            return;
        }

        const size_t activeButtonStates = getButtonStates(port);
        for (const auto &button : myButtonRemapped)
        {
            const auto &descriptor = button.first;
            if (activeButtonStates & (1 << descriptor.id))
            {
                const char key = button.second;
                addKeyToBuffer(key);
            }
        }
    }

    void InputRemapper::mouseEmulation(unsigned port, unsigned device, const double speed)
    {
        if (device != RETRO_DEVICE_MOUSE)
        {
            return;
        }

        for (auto &mouse : myMouse)
        {
            const int16_t x =
                input_state_cb(port, mouse.descriptor.device, mouse.descriptor.index, mouse.descriptor.id);
            mouse.positions[port] += x * mouse.multiplier * speed;
            mouse.positions[port] = std::min(1.0, std::max(mouse.positions[port], -1.0));
        }
    }

} // namespace ra2
