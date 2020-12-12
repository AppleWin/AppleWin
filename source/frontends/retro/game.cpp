#include "StdAfx.h"
#include "frontends/retro/game.h"

#include "Frame.h"
#include "Video.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "Mockingboard.h"
#include "SoundCore.h"
#include "Harddisk.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "NTSC.h"
#include "SaveState.h"
#include "RGBMonitor.h"
#include "Riff.h"
#include "Utilities.h"

#include "linux/videobuffer.h"
#include "linux/keyboard.h"
#include "linux/paddle.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/configuration.h"
#include "frontends/retro/environment.h"

#include "libretro.h"

namespace
{

  void initialiseEmulator()
  {
#ifdef RIFF_SPKR
    RiffInitWriteFile("/tmp/Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
    RiffInitWriteFile("/tmp/Mockingboard.wav", 44100, 2);
#endif

    g_nAppMode = MODE_RUNNING;
    LogFileOutput("Initialisation\n");

    g_bFullSpeed = false;

    LoadConfiguration();
    SetCurrentCLK6502();
    GetAppleWindowTitle();

    DSInit();
    MB_Initialize();
    SpkrInitialize();

    MemInitialize();
    VideoBufferInitialize();
    VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());

    GetCardMgr().GetDisk2CardMgr().Reset();
    HD_Reset();
    Snapshot_Startup();
  }

  void uninitialiseEmulator()
  {
    Snapshot_Shutdown();
    CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
    if (pMouseCard)
    {
      pMouseCard->Reset();
    }
    VideoBufferDestroy();
    MemDestroy();

    SpkrDestroy();
    MB_Destroy();
    DSUninit();

    HD_Destroy();
    PrintDestroy();
    CpuDestroy();

    GetCardMgr().GetDisk2CardMgr().Destroy();
    LogDone();
    RiffFinishWriteFile();
  }

  void runOneFrame(Speed & speed)
  {
    if (g_nAppMode == MODE_RUNNING)
    {
      const size_t cyclesToExecute = speed.getCyclesTillNext(16);

      const bool bVideoUpdate = true;
      const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

      const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);
      //      log_cb(RETRO_LOG_INFO, "RA2: %s %d\n", __FUNCTION__, executedCycles);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
      GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedCycles);
      MB_PeriodicUpdate(executedCycles);
      SpkrUpdate(executedCycles);
    }
  }

}

Game::Game() : mySpeed(true)
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

void Game::drawVideoBuffer()
{
  const size_t pitch = GetFrameBufferWidth() * 4;
  video_cb(g_pFramebufferbits, GetFrameBufferWidth(), GetFrameBufferHeight(), pitch);
}

bool Game::loadGame(const char * path)
{
  const bool ok = DoDiskInsert(SLOT6, DRIVE_1, path);
  return ok;
}
