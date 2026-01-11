#include "StdAfx.h"
#include "frontends/sdl/gamepad.h"
#include "frontends/sdl/utils.h"

#include <iostream>
#include <stdexcept>

#define AXIS_MIN -32768
#define AXIS_MAX 32767

namespace sa2
{

    std::shared_ptr<Gamepad> Gamepad::create(const std::optional<int> &index, const std::string &mappingFile)
    {
        const std::vector<Joystick_t> joysticks = compat::getGameControllers();
        if (!index && joysticks.empty())
        {
            // default and no joysticks available
            return std::shared_ptr<Gamepad>();
        }

        if (!mappingFile.empty())
        {
            const int res = SDL_GameControllerAddMappingsFromFile(mappingFile.c_str());
            if (res < 0)
            {
                throw std::runtime_error(decorateSDLError("SDL_GameControllerAddMappingsFromFile"));
            }
            std::cerr << "SDL Game Controller: Loaded " << res << " mappings" << std::endl;
        }

        const int ind = index.value_or(0);
        if (ind < 0 || size_t(ind) >= joysticks.size())
        {
            throw std::runtime_error("Requested game controller index is out of range");
        }

        try
        {
            const Joystick_t id = joysticks[ind];
            return std::make_shared<Gamepad>(id);
        }
        catch (const std::runtime_error &e)
        {
            if (!index)
            {
                // first game controller exists, but it is not usable
                // since it was not explicitly required, do not throw
                std::cerr << e.what() << std::endl;
                return std::shared_ptr<Gamepad>();
            }
            throw;
        }
    }

    Gamepad::Gamepad(const Joystick_t id)
        : myButtonCodes(2)
        , myAxisCodes(2)
    {
        myController.reset(SDL_GameControllerOpen(id), SDL_GameControllerClose);
        if (!myController)
        {
            throw std::runtime_error(decorateSDLError("SDL_GameControllerOpen"));
        }
        std::cerr << "SDL Game Controller: " << SDL_GameControllerName(myController.get()) << std::endl;

        myButtonCodes[0] = SDL_CONTROLLER_BUTTON_A;
        myButtonCodes[1] = SDL_CONTROLLER_BUTTON_B;
        myAxisCodes[0] = SDL_CONTROLLER_AXIS_LEFTX;
        myAxisCodes[1] = SDL_CONTROLLER_AXIS_LEFTY;
    }

    bool Gamepad::getButton(int i) const
    {
        int value = 0;
        if (myController)
        {
            value = SDL_GameControllerGetButton(myController.get(), myButtonCodes[i]);
        }
        return value != 0;
    }

    double Gamepad::getAxis(int i) const
    {
        if (myController)
        {
            const int value = SDL_GameControllerGetAxis(myController.get(), myAxisCodes[i]);
            const double axis = 2.0 * double(value - AXIS_MIN) / double(AXIS_MAX - AXIS_MIN) - 1.0;
            return axis;
        }
        else
        {
            return 0.0;
        }
    }

} // namespace sa2
