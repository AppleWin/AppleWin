#include "StdAfx.h"
#include "frontends/retro/game.h"

#include "Frame.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Mockingboard.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "NTSC.h"
#include "Utilities.h"
#include "Video.h"
#include "Interface.h"

#include "linux/keyboard.h"
#include "linux/paddle.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/configuration.h"
#include "frontends/common2/utils.h"

#include "libretro.h"

namespace
{

  void runOneFrame(Speed & speed)
  {
    if (g_nAppMode == MODE_RUNNING)
    {
      const size_t cyclesToExecute = speed.getCyclesTillNext(16);

      const bool bVideoUpdate = true;
      const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

      const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
      GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedCycles);
      MB_PeriodicUpdate(executedCycles);
      SpkrUpdate(executedCycles);
    }
  }

  void updateWindowTitle()
  {
    GetAppleWindowTitle();
    display_message(g_pAppTitle.c_str());
  }

}

unsigned Game::input_devices[MAX_PADS] = {0};

Game::Game()
  : mySpeed(true), myButtonStates(RETRO_DEVICE_ID_JOYPAD_R3 + 1)
{
  EmulatorOptions options;
  options.memclear = g_nMemoryClearType;
  options.log = true;

  if (options.log)
  {
    LogInit();
  }

  InitializeRegistry(options);
  initialiseEmulator();

  myBorderlessWidth = GetFrameBufferBorderlessWidth();
  myBorderlessHeight = GetFrameBufferBorderlessHeight();
  const size_t borderWidth = GetFrameBufferBorderWidth();
  const size_t borderHeight = GetFrameBufferBorderHeight();
  const size_t width = GetFrameBufferWidth();
  myHeight = GetFrameBufferHeight();

  myPitch = width * sizeof(bgra_t);
  myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

  const size_t size = myHeight * myPitch;
  myVideoBuffer.resize(size);
}

Game::~Game()
{
  uninitialiseEmulator();
}

void Game::executeOneFrame()
{
  runOneFrame(mySpeed);
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
  if (input_devices[0] != RETRO_DEVICE_NONE)
  {
    if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_R))
    {
      g_eVideoType++;
      if (g_eVideoType >= NUM_VIDEO_MODES)
	g_eVideoType = 0;

      Config_Save_Video();
      VideoReinitialize();
      VideoRedrawScreen();
      updateWindowTitle();
    }
    if (checkButtonPressed(RETRO_DEVICE_ID_JOYPAD_L))
    {
      VideoStyle_e videoStyle = GetVideoStyle();
      videoStyle = VideoStyle_e(videoStyle ^ VS_HALF_SCANLINES);

      SetVideoStyle(videoStyle);

      Config_Save_Video();
      VideoReinitialize();
      VideoRedrawScreen();
      updateWindowTitle();
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

void Game::drawVideoBuffer()
{
  // this should not be necessary
  // either libretro handles it
  // or we should change AW
  // but for now, there is no alternative
  for (size_t row = 0; row < myHeight; ++row)
  {
    const uint8_t * src = g_pFramebufferbits + row * myPitch;
    uint8_t * dst = myVideoBuffer.data() + (myHeight - row - 1) * myPitch;
    memcpy(dst, src, myPitch);
  }

  video_cb(myVideoBuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
}

bool Game::loadGame(const char * path)
{
  const bool ok = DoDiskInsert(SLOT6, DRIVE_1, path);
  return ok;
}
