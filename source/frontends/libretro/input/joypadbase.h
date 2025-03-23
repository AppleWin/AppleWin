#pragma once

#include "linux/paddle.h"
#include "frontends/libretro/input/inputremapper.h"

#include <memory>
#include <vector>

namespace ra2
{

    class Game;

    class ControllerBase : public Paddle
    {
    public:
        ControllerBase(
            const std::unique_ptr<Game> &game, const unsigned port, const std::vector<InputDescriptor> &buttonCodes);

        bool getButton(int i) const override;

    protected:
        int16_t inputState(const InputDescriptor &descriptor) const;
        const std::unique_ptr<Game> &myGame;
        const unsigned myPort;

    private:
        const std::vector<InputDescriptor> myButtonCodes;
    };

    class JoypadBase : public ControllerBase
    {
    public:
        JoypadBase(const std::unique_ptr<Game> &game, unsigned port);
    };

} // namespace ra2
