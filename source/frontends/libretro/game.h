#pragma once

#include "frontends/common2/controllerdoublepress.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/diskcontrol.h"
#include "frontends/libretro/rkeyboard.h"

#include "Common.h"

#include <memory>
#include <string>
#include <vector>

class LoggerContext;
class RegistryContext;

namespace common2
{
    class PTreeRegistry;
}

namespace ra2
{

    class RetroFrame;

    class Game
    {
    public:
        Game(const bool supportsInputBitmasks);
        ~Game();

        bool loadSnapshot(const std::string &path);

        void reset();

        void updateVariables();
        void executeOneFrame();
        void processInputEvents();
        void writeAudio(const size_t fps, const size_t sampleRate, const size_t channels);

        void drawVideoBuffer();

        double getMousePosition(int i) const;

        DiskControl &getDiskControl();

        void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

        static constexpr size_t FPS = 60;
        static constexpr size_t SAMPLE_RATE = SPKR_SAMPLE_RATE;
        static constexpr size_t CHANNELS = 2;

        static unsigned ourInputDevices[MAX_PADS];
        static constexpr retro_usec_t ourFrameTime = 1000000 / FPS;

    private:
        const bool mySupportsInputBitmasks;
        size_t myButtonStates;
        KeyboardType myKeyboardType;

        // keep them in this order!
        std::shared_ptr<LoggerContext> myLoggerContext;
        std::shared_ptr<common2::PTreeRegistry> myRegistry;
        std::shared_ptr<RegistryContext> myRegistryContext;
        std::shared_ptr<RetroFrame> myFrame;

        common2::ControllerDoublePress myControllerQuit;
        common2::ControllerDoublePress myControllerReset;

        struct MousePosition_t
        {
            double position; // -1 to 1
            double multiplier;
            unsigned id;
        };

        MousePosition_t myMouse[2];

        DiskControl myDiskControl;

        std::vector<int16_t> myAudioBuffer;

        size_t updateButtonStates();
        void keyboardEmulation();
        void mouseEmulation();
        void refreshVariables();

        void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
        void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
    };

} // namespace ra2
