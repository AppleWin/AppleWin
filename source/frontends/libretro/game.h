#pragma once

#include "linux/context.h"
#include "frontends/common2/controllerdoublepress.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/diskcontrol.h"
#include "frontends/libretro/input/rkeyboard.h"
#include "frontends/libretro/input/inputremapper.h"

#include <memory>
#include <string>
#include <vector>

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

        void start();
        void restart();

        void updateVariables();
        void checkForMemoryWrites();
        void executeOneFrame();
        void processInputEvents();
        void writeAudio(const size_t fps, const size_t sampleRate, const size_t channels);
        void flushMemory();

        void drawVideoBuffer();

        common2::PTreeRegistry &getRegistry();
        DiskControl &getDiskControl();
        InputRemapper &getInputRemapper();

        void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

        static constexpr size_t FPS = 60;
        static constexpr size_t SAMPLE_RATE = SPKR_SAMPLE_RATE;
        static constexpr size_t CHANNELS = 2;

        static unsigned ourInputDevices[MAX_PADS];
        static constexpr retro_usec_t ourFrameTime = 1000000 / FPS;

    private:
        KeyboardType myKeyboardType;
        double myMouseSpeed;

        // keep them in this order!
        std::unique_ptr<LoggerContext> myLoggerContext;
        std::shared_ptr<common2::PTreeRegistry> myRegistry;
        std::unique_ptr<RegistryContext> myRegistryContext;
        std::shared_ptr<RetroFrame> myFrame;

        common2::ControllerDoublePress myControllerQuit;
        common2::ControllerDoublePress myControllerReset;

        InputRemapper myInputRemapper;

        DiskControl myDiskControl;

        std::vector<int16_t> myAudioBuffer;
        LPBYTE myMainMemoryReference;

        void keyboardEmulation();
        void applyVariables();

        void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
        void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
    };

} // namespace ra2
