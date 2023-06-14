#include "StdAfx.h"
#include "frontends/libretro/game.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/retroframe.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/ptreeregistry.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Speaker.h"
#include "CPU.h"
#include "NTSC.h"
#include "Interface.h"

#include "linux/keyboardbuffer.h"
#include "linux/paddle.h"
#include "linux/context.h"

#include "libretro.h"
#include <memory>

#define APPLEWIN_RETRO_CONF "/tmp/applewin.retro.conf"

namespace
{
  void saveRegistryToINI(const std::shared_ptr<common2::PTreeRegistry> & registry)
  {
    try
    {
      registry->saveToINIFile(APPLEWIN_RETRO_CONF);
      ra2::display_message("Configuration saved to: " APPLEWIN_RETRO_CONF);
    } catch (const std::exception & e) {
      ra2::display_message(std::string("Error saving configuration: ") + e.what());
    }
  }
}

namespace ra2
{

  unsigned Game::ourInputDevices[MAX_PADS] = {RETRO_DEVICE_NONE};

  Game::Game()
    : mySpeed(true)  // fixed speed
    , myButtonStates(RETRO_DEVICE_ID_JOYPAD_R3 + 1)
  {
    myLoggerContext = std::make_shared<LoggerContext>(true);
    myRegistry = CreateRetroRegistry();
    myRegistryContext = std::make_shared<RegistryContext>(myRegistry);
    myFrame = std::make_shared<ra2::RetroFrame>();

    myAudioChannelsSelected = GetAudioOutputChannels();

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

  retro_usec_t Game::ourFrameTime = 0;

  void Game::executeOneFrame()
  {
    if (g_nAppMode == MODE_RUNNING)
    {
      const bool bVideoUpdate = true;
      const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

      const DWORD cyclesToExecute = mySpeed.getCyclesTillNext(ourFrameTime);
      const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
      GetCardMgr().Update(executedCycles);
      SpkrUpdate(executedCycles);
    }
  }

  void Game::updateVariables()
  {
    bool updated = false;
    if (ra2::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
    {
      PopulateRegistry(myRegistry);

      // some variables are immediately applied
      myAudioChannelsSelected = GetAudioOutputChannels();

      Video& video = GetVideo();
      const VideoType_e prevVideoType = video.GetVideoType();
      const VideoStyle_e prevVideoStyle = video.GetVideoStyle();

      DWORD dwTmp = prevVideoType;
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

  void Game::frameTimeCallback(retro_usec_t usec)
  {
    ourFrameTime = usec;
  }

  void Game::processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers)
  {
    BYTE ch = 0;
    switch (keycode)
    {
    case RETROK_RETURN:
      {
        ch = 0x0d;
        break;
      }
    case RETROK_BACKSPACE: // same as AppleWin
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
    case RETROK_DELETE:
      {
        ch = 0x7f;
        break;
      }
    case RETROK_ESCAPE:
      {
        ch = 0x1b;
        break;
      }
    case RETROK_TAB:
      {
        ch = 0x09;
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

    if (!ch)
    {
      switch (character) {
      case 0x20 ... 0x40:
      case 0x5b ... 0x60:
      case 0x7b ... 0x7e:
        {
          // not the letters
          // this is very simple, but one cannot handle CRTL-key combination.
          ch = character;
          break;
        }
      }
    }

    if (ch)
    {
      addKeyToBuffer(ch);
      // log_cb(RETRO_LOG_INFO, "RA2: %s - %02x\n", __FUNCTION__, ch);
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

  bool Game::checkButtonPressed(unsigned id)
  {
    // pressed if it is down now, but was up before
    const int value = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id);
    const bool pressed = (value != 0) && myButtonStates[id] == 0;

    // update to avoid multiple fires
    myButtonStates[id] = value;

    return pressed;
  }

  void Game::keyboardEmulation()
  {
    if (ourInputDevices[0] != RETRO_DEVICE_NONE)
    {
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_R))
      {
        myFrame->CycleVideoType();
      }
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_L))
      {
        myFrame->Cycle50ScanLines();
      }
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_L2))
      {
        saveRegistryToINI(myRegistry);
      }
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_START))
      {
        reset(); // just a myFrame->Restart();
      }
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_SELECT))
      {
        // added as convenience if game_focus is on:
        // exit emulator by pressing "select" twice
        const auto secondBtnPress = std::chrono::steady_clock::now();
        const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(secondBtnPress - myFirstBtnPress).count();
        myFirstBtnPress = secondBtnPress;
        if (dt <= 500)
        {
          log_cb(RETRO_LOG_INFO, "RA2: %s - user quitted\n", __FUNCTION__);
          environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
        }
        else
        {
          display_message("Press again to quit...", 30 /* 0.5s at 60 FPS */);
        }
      }
    }
    else
    {
      std::fill(myButtonStates.begin(), myButtonStates.end(), 0);
    }
  }

  void Game::mouseEmulation()
  {
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
    ra2::writeAudio(myAudioChannelsSelected, fps);
  }

}
