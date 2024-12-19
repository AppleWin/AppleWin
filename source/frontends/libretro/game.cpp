#include "StdAfx.h"
#include "frontends/libretro/game.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/retroframe.h"
#include "frontends/libretro/rkeyboard.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"

#include "Common.h"
#include "Interface.h"

#include "linux/keyboardbuffer.h"
#include "linux/paddle.h"
#include "linux/context.h"

#include "libretro.h"

#define APPLEWIN_RETRO_CONF "/tmp/applewin.retro.conf"

namespace
{

  void saveRegistryToINI(const std::shared_ptr<common2::PTreeRegistry> & registry)
  {
    try
    {
      registry->saveToINIFile(APPLEWIN_RETRO_CONF);
      ra2::display_message("Configuration saved to: " APPLEWIN_RETRO_CONF);
    }
    catch (const std::exception & e)
    {
      ra2::display_message(std::string("Error saving configuration: ") + e.what());
    }
  }

}

namespace ra2
{

  unsigned Game::ourInputDevices[MAX_PADS] = {RETRO_DEVICE_NONE};

  Game::Game(const bool supportsInputBitmasks)
    : mySupportsInputBitmasks(supportsInputBitmasks)
    , myButtonStates(0)
    , myAudioSource(AudioSource::UNKNOWN)
    , myKeyboardType(KeyboardType::ASCII)
  {
    myLoggerContext = std::make_shared<LoggerContext>(true);
    myRegistry = CreateRetroRegistry();
    myRegistryContext = std::make_shared<RegistryContext>(myRegistry);

    common2::EmulatorOptions defaultOptions;
    defaultOptions.fixedSpeed = true;
    myFrame = std::make_shared<ra2::RetroFrame>(defaultOptions);

    refreshVariables();

    SetFrame(myFrame);
    myFrame->Begin();

    Video & video = GetVideo();
    // should the user be allowed to tweak 0.75
    myMouse[0] = {0.0, 0.75 / video.GetFrameBufferBorderlessWidth(), RETRO_DEVICE_ID_MOUSE_X};
    myMouse[1] = {0.0, 0.75 / video.GetFrameBufferBorderlessHeight(), RETRO_DEVICE_ID_MOUSE_Y};
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

  void Game::refreshVariables()
  {
    myAudioSource = GetAudioSource();
    myKeyboardType = GetKeyboardEmulationType();
  }

  void Game::updateVariables()
  {
    bool updated = false;
    if (ra2::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
    {
      PopulateRegistry(myRegistry);

      // some variables are immediately applied
      refreshVariables();

      Video& video = GetVideo();
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
    keyboardEmulation();
    mouseEmulation();
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
      switch (character) {
      case 0x20 ... 0x40:   // space ... @
      case 0x5b ... 0x60:   // [ ... `
      case 0x7b ... 0x7e:   // { ... ~
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

    // log_cb(RETRO_LOG_INFO, "RA2: %s - char=%02x keycode=%04x modifiers=%02x ch=%02x\n", __FUNCTION__, character, keycode, key_modifiers, ch);

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

  size_t Game::updateButtonStates()
  {
    size_t newState;
    if (mySupportsInputBitmasks)
    {
      newState = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK);
    }
    else
    {
      newState = 0;
      for (size_t i = 0; i < RETRO_DEVICE_ID_JOYPAD_R3 + 1; i++)
      {
        if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
        {
          newState |= 1 << i;
        }
      }
    }
    // if it is active NOW and was NOT before.
    const size_t result = (~myButtonStates) & newState;

    myButtonStates = newState;
    return result;
  }

  void Game::keyboardEmulation()
  {
    if (ourInputDevices[0] != RETRO_DEVICE_NONE)
    {
      const size_t activeButtons = updateButtonStates();

      const auto checkButton = [activeButtons] (const size_t i)
      {
        return activeButtons & (1 << i);
      };

      if (checkButton(RETRO_DEVICE_ID_JOYPAD_R))
      {
        myFrame->CycleVideoType();
      }
      if (checkButton(RETRO_DEVICE_ID_JOYPAD_L))
      {
        myFrame->Cycle50ScanLines();
      }
      if (checkButton(RETRO_DEVICE_ID_JOYPAD_L2))
      {
        saveRegistryToINI(myRegistry);
      }
      if (checkButton(RETRO_DEVICE_ID_JOYPAD_R2))
      {
        switch (myAudioSource)
        {
        case AudioSource::SPEAKER:
        {
          myAudioSource = AudioSource::MOCKINGBOARD;
          display_message(std::string("Audio source: ") + REGVALUE_AUDIO_MOCKINGBOARD);
          break;
        }
        case AudioSource::MOCKINGBOARD:
        {
          myAudioSource = AudioSource::SPEAKER;
          display_message(std::string("Audio source: ") + REGVALUE_AUDIO_SPEAKER);
          break;
        }
        }
      }
      if (checkButton(RETRO_DEVICE_ID_JOYPAD_START))
      {
        // reset emulator by pressing "start" twice
        if (myControllerReset.pressButton())
        {
          reset(); // just a myFrame->Restart();
        }
        else
        {
          display_message("Press again to reset...", 60 /* 1.0s at 60 FPS */);
        }
      }
      if (checkButton(RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
        // added as convenience if game_focus is on:
        // exit emulator by pressing "select" twice
        if (myControllerQuit.pressButton())
        {
          log_cb(RETRO_LOG_INFO, "RA2: %s - user quitted\n", __FUNCTION__);
          environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
        }
        else
        {
          display_message("Press again to quit...", 60 /* 1.0s at 60 FPS */);
        }
      }
    }
    else
    {
      myButtonStates = 0;
    }
  }

  void Game::mouseEmulation()
  {
    // we should use an InputDescriptor, but these are all on RETRO_DEVICE_MOUSE anyway
    for (auto & mouse : myMouse)
    {
      const int16_t x = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, mouse.id);
      mouse.position += x * mouse.multiplier;
      mouse.position = std::min(1.0, std::max(mouse.position, -1.0));
    }
  }

  double Game::getMousePosition(int i) const
  {
    return myMouse[i].position;
  }

  bool Game::loadSnapshot(const std::string & path)
  {
    const common2::RestoreCurrentDirectory restoreChDir;
    common2::setSnapshotFilename(path);
    myFrame->LoadSnapshot();
    return true;
  }

  DiskControl & Game::getDiskControl()
  {
    return myDiskControl;
  }

  void Game::reset()
  {
    myFrame->Restart();
  }

  void Game::writeAudio(const size_t fps)
  {
    ra2::writeAudio(myAudioSource, fps);
  }

}
