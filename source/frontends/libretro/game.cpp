#include "StdAfx.h"
#include "frontends/libretro/game.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/retroframe.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/input/rkeyboard.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"

#include "Registry.h"
#include "Interface.h"
#include "Memory.h"

#include "linux/keyboardbuffer.h"
#include "linux/paddle.h"
#include "linux/context.h"

#include "libretro.h"

#define APPLEWIN_RETRO_CONF "/tmp/applewin.retro.conf"

namespace
{

    void saveRegistryToINI(const common2::PTreeRegistry &registry)
    {
        try
        {
            registry.saveToINIFile(APPLEWIN_RETRO_CONF);
            ra2::display_message("Configuration saved to: " APPLEWIN_RETRO_CONF);
        }
        catch (const std::exception &e)
        {
            ra2::display_message(std::string("Error saving configuration: ") + e.what());
        }
    }

} // namespace

namespace ra2
{

    unsigned Game::ourInputDevices[MAX_PADS] = {};

    Game::Game(const bool supportsInputBitmasks)
        : myInputRemapper(supportsInputBitmasks)
        , myKeyboardType(KeyboardType::ASCII)
        , myMouseSpeed(1.0)
        , myMainMemoryReference(nullptr)
    {
        myLoggerContext = std::make_unique<LoggerContext>(true);
        myRegistry = createRetroRegistry();
        myRegistryContext = std::make_unique<RegistryContext>(myRegistry);

        applyVariables();

        common2::EmulatorOptions defaultOptions;
        defaultOptions.fixedSpeed = true;
        myFrame = std::make_shared<ra2::RetroFrame>(defaultOptions);

        SetFrame(myFrame);
    }

    Game::~Game()
    {
        myFrame->End();
        myFrame.reset();
        SetFrame(myFrame);
    }

    void Game::executeOneFrame()
    {
        myFrame->ExecuteOneFrame(ourFrameTime);
    }

    void Game::applyVariables()
    {
        applyRetroVariables(*this);

        myKeyboardType = getKeyboardEmulationType();
        myMouseSpeed = getMouseSpeed();
    }

    void Game::updateVariables()
    {
        bool updated = false;
        if (ra2::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
        {
            applyVariables();

            // apply video mode changes
            Video &video = GetVideo();
            const VideoType_e prevVideoType = video.GetVideoType();
            const VideoStyle_e prevVideoStyle = video.GetVideoStyle();

            uint32_t dwTmp = prevVideoType;
            RegLoadValue(REG_CONFIG, REGVALUE_VIDEO_MODE, TRUE, &dwTmp);
            const VideoType_e newVideoType = static_cast<VideoType_e>(dwTmp);

            dwTmp = prevVideoStyle;
            RegLoadValue(REG_CONFIG, REGVALUE_VIDEO_STYLE, TRUE, &dwTmp);
            const VideoStyle_e newVideoStyle = static_cast<VideoStyle_e>(dwTmp);

            if ((prevVideoType != newVideoType) || (prevVideoStyle != newVideoStyle))
            {
                video.SetVideoStyle(newVideoStyle);
                video.SetVideoType(newVideoType);
                myFrame->ApplyVideoModeChange();
            }
        }
    }

    void Game::processInputEvents()
    {
        input_poll_cb();
        for (size_t port = 0; port < MAX_PADS; ++port)
        {
            const unsigned device = ourInputDevices[port];
            myInputRemapper.mouseEmulation(port, device, myMouseSpeed);
            myInputRemapper.processRemappedButtons(port, device);
        }
    }

    void Game::keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
    {
        if (down)
        {
            processKeyDown(keycode, character, key_modifiers);
        }
        else
        {
            processKeyUp(keycode, character, key_modifiers);
        }
    }

    void Game::processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers)
    {
        BYTE ch = 0;
        bool found = false; // it seems CTRL-2 produces NUL. is it a real key?
        bool valid = true;

        if (myKeyboardType == KeyboardType::ASCII)
        {
            // if we tried ASCII and the character is invalid, dont try again with scancode (e.g. £)
            valid = character < 0x80;

            // this block is to ensure the ascii character is used
            // if selected and available
            switch (character)
            {
            case 0x20 ... 0x40: // space ... @
            case 0x5b ... 0x60: // [ ... `
            case 0x7b ... 0x7e: // { ... ~
            {
                ch = character;
                found = true;
                break;
            }
            }
        }

        if (!found && valid)
        {
            // use scancodes, but dont overwrite invalid ASCII
            found = getApple2Character(keycode, key_modifiers & RETROKMOD_CTRL, key_modifiers & RETROKMOD_SHIFT, ch);
        }

        if (!found)
        {
            // some special characters and letters
            switch (keycode)
            {
            case RETROK_LEFT:
            {
                ch = 0x08;
                break;
            }
            case RETROK_RIGHT:
            {
                ch = 0x15;
                break;
            }
            case RETROK_UP:
            {
                ch = 0x0b;
                break;
            }
            case RETROK_DOWN:
            {
                ch = 0x0a;
                break;
            }
            case RETROK_END:
            {
                // reset the emulator (like vice's default key for vice_mapper_reset)
                restart();
                break;
            }
            case RETROK_HOME:
            {
                // save the registry to a file
                saveRegistryToINI(*myRegistry);
                break;
            }
            case RETROK_LALT:
            {
                Paddle::setButtonPressed(Paddle::ourOpenApple);
                break;
            }
            case RETROK_RALT:
            {
                Paddle::setButtonPressed(Paddle::ourSolidApple);
                break;
            }
            case RETROK_a ... RETROK_z:
            {
                ch = (keycode - RETROK_a) + 0x01;
                if (key_modifiers & RETROKMOD_CTRL)
                {
                    // ok
                }
                else if (key_modifiers & RETROKMOD_SHIFT)
                {
                    ch += 0x60;
                }
                else
                {
                    ch += 0x40;
                }
                break;
            }
            }
            found = !!ch;
        }

        // log_cb(RETRO_LOG_INFO, "RA2: %s - char=%02x keycode=%04x modifiers=%02x ch=%02x\n", __FUNCTION__, character,
        // keycode, key_modifiers, ch);

        if (found)
        {
            addKeyToBuffer(ch);
        }
    }

    void Game::processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers)
    {
        switch (keycode)
        {
        case RETROK_LALT:
        {
            Paddle::setButtonReleased(Paddle::ourOpenApple);
            break;
        }
        case RETROK_RALT:
        {
            Paddle::setButtonReleased(Paddle::ourSolidApple);
            break;
        }
        }
    }

    bool Game::loadSnapshot(const std::string &path)
    {
        const common2::RestoreCurrentDirectory restoreChDir;
        common2::setSnapshotFilename(path);
        myFrame->LoadSnapshot();
        return true;
    }

    DiskControl &Game::getDiskControl()
    {
        return myDiskControl;
    }

    void Game::start()
    {
        myFrame->Begin();

        // memmain is not exposed, but is returned by this function, so store it for later use
        myMainMemoryReference = MemGetBankPtr(0, true);
    }

    void Game::restart()
    {
        myFrame->Restart();
    }

    void Game::writeAudio(const size_t fps, const size_t sampleRate, const size_t channels)
    {
        ra2::writeAudio(fps, sampleRate, channels, myAudioBuffer);
    }

    common2::PTreeRegistry &Game::getRegistry()
    {
        return *myRegistry;
    }

    InputRemapper &Game::getInputRemapper()
    {
        return myInputRemapper;
    }

    void Game::flushMemory()
    {
        // if not using shadow areas, all reads/writes will occur directly on memmain
        if (!GetIsMemCacheValid())
            return;

        // force flush (mem -> memmain) so frontend can see the current state of memory
        MemGetBankPtr(0, true);
    }

    void Game::checkForMemoryWrites()
    {
        // if not using shadow areas, all reads/writes will occur directly on memmain
        if (!GetIsMemCacheValid())
            return;

        // the libretro interface exposes memmain. for any pages that have a copy in mem,
        // copy the memmain back into mem in case it was changed between frames (by cheats,
        // debuggers, or other forms of memory editing)
        LPBYTE memMainPtr = myMainMemoryReference;
        for (UINT loop = 0; loop < _6502_NUM_PAGES; loop++)
        {
            LPBYTE altptr = MemGetMainPtr(loop * _6502_PAGE_SIZE);
            if (altptr != memMainPtr)
            {
                // because this ensures mem and memmain match, we don't have to set the dirty flag
                memcpy(altptr, memMainPtr, _6502_PAGE_SIZE);
            }

            memMainPtr += _6502_PAGE_SIZE;
        }
    }

} // namespace ra2
