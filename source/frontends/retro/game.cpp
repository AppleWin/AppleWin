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

  std::unordered_map<unsigned, BYTE> getKeymap()
  {
    std::unordered_map<unsigned, BYTE> km;
    for (unsigned key = RETROK_a; key <= RETROK_z; ++key)
    {
      km[key] = 0x41 + (key - RETROK_a);
    }
    for (unsigned key = RETROK_0; key <= RETROK_9; ++key)
    {
      km[key] = 0x30 + (key - RETROK_0);
    }
    km[RETROK_SPACE] = 0x20;
    km[RETROK_RETURN] = 0x0d;
    km[RETROK_UP] = 0x0b;
    km[RETROK_DOWN] = 0x0a;
    km[RETROK_LEFT] = 0x08;
    km[RETROK_UP] = 0x15;
    km[RETROK_DELETE] = 0x7f;
    km[RETROK_BACKSPACE] = 0x08;
    km[RETROK_ESCAPE] = 0x1b;

    return km;
  }

}

Game::Game() : myKeymap(getKeymap()), mySpeed(true)
{
  EmulatorOptions options;
  options.memclear = g_nMemoryClearType;
  options.log = true;
  options.useQtIni = true;

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
  processKeyboardEvents();
}

void Game::processKeyboardEvents()
{
  std::vector<unsigned> newKeys;

  for (auto it : myKeymap)
  {
    const unsigned key = it.first;
    const int16_t isDown = input_state_cb(0, RETRO_DEVICE_KEYBOARD, 0, key);

    // pressed now, not pressed before
    if (isDown && !myKeystate[key])
    {
      // add
      newKeys.push_back(it.second);
    }

    myKeystate[key] = isDown != 0;
  }

  // pass to AppleWin. in which order?
  for (unsigned key : newKeys)
  {
    addKeyToBuffer(key);
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
