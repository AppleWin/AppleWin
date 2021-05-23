#include "StdAfx.h"
#include "frontends/libretro/game.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/joypad.h"
#include "frontends/libretro/analog.h"
#include "frontends/libretro/retroframe.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Mockingboard.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "NTSC.h"
#include "Utilities.h"

#include "linux/keyboard.h"
#include "linux/registry.h"
#include "linux/paddle.h"
#include "linux/context.h"
#include "frontends/common2/utils.h"

#include "libretro.h"

namespace
{

  bool insertDisk(const std::string & filename)
  {
    if (filename.empty())
    {
      return false;
    }

    Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6));
    const ImageError_e error = disk2Card.InsertDisk(DRIVE_1, filename.c_str(), IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE);

    if (error == eIMAGE_ERROR_NONE)
    {
      return true;
    }

    // try a hard disk
    HD_SetEnabled(true);
    BOOL bRes = HD_Insert(HARDDISK_1, filename);
    return bRes == TRUE;
  }

}

namespace ra2
{

  unsigned Game::ourInputDevices[MAX_PADS] = {RETRO_DEVICE_NONE};

  Game::Game()
    : myLogger(true)
    , myFrame(new ra2::RetroFrame())
    , mySpeed(true)
    , myButtonStates(RETRO_DEVICE_ID_JOYPAD_R3 + 1)
  {
    InitialiseRetroRegistry();
    SetFrame(myFrame);
    myFrame->Initialize();

    switch (ourInputDevices[0])
    {
    case RETRO_DEVICE_NONE:
      Paddle::instance.reset();
      break;
    case RETRO_DEVICE_JOYPAD:
      Paddle::instance.reset(new Joypad);
      Paddle::setSquaring(false);
      break;
    case RETRO_DEVICE_ANALOG:
      Paddle::instance.reset(new Analog);
      Paddle::setSquaring(true);
      break;
    default:
      break;
    }
  }

  Game::~Game()
  {
    myFrame->Destroy();
    SetFrame(std::shared_ptr<FrameBase>());
    Paddle::instance.reset();
    Registry::instance.reset();
  }

  retro_usec_t Game::ourFrameTime = 0;

  void Game::executeOneFrame()
  {
    if (g_nAppMode == MODE_RUNNING)
    {
      const bool bVideoUpdate = true;
      const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

      const uint64_t cyclesToExecute = mySpeed.getCyclesTillNext(ourFrameTime);
      const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
      GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedCycles);
      MB_PeriodicUpdate(executedCycles);
      SpkrUpdate(executedCycles);
    }
  }

  void Game::processInputEvents()
  {
    input_poll_cb();
    keyboardEmulation();
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
      log_cb(RETRO_LOG_INFO, "RA2: %s - %02x\n", __FUNCTION__, ch);
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
      if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_START))
      {
        ResetMachineState();
      }
    }
    else
    {
      std::fill(myButtonStates.begin(), myButtonStates.end(), 0);
    }
  }

  bool Game::loadGame(const std::string & path)
  {
    const bool ok = insertDisk(path);
    return ok;
  }

  bool Game::loadSnapshot(const std::string & path)
  {
    common2::setSnapshotFilename(path, true);
    return true;
  }

}
