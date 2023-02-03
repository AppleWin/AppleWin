#include "StdAfx.h"

#include "linux/context.h"
#include "linux/registry.h"
#include "linux/paddle.h"
#include "linux/duplicates/PropertySheet.h"

#include "Debugger/Debug.h"

#include "Interface.h"
#include "CardManager.h"
#include "Log.h"
#include "Utilities.h"
#include "SoundCore.h"
#include "CPU.h"
#include "Riff.h"
#include "SaveState.h"
#include "Memory.h"
#include "Speaker.h"


namespace
{
  std::shared_ptr<FrameBase> sg_LinuxFrame;
}

IPropertySheet& GetPropertySheet()
{
  static CPropertySheet sg_PropertySheet;
  return sg_PropertySheet;
}

FrameBase& GetFrame()
{
  return *sg_LinuxFrame;
}

void SetFrame(const std::shared_ptr<FrameBase> & frame)
{
  sg_LinuxFrame = frame;
}

Video& GetVideo()
{
  static Video sg_Video;
  return sg_Video;
}

Initialisation::Initialisation(
  const std::shared_ptr<FrameBase> & frame,
  const std::shared_ptr<Paddle> & paddle
  )
{
  SetFrame(frame);
  Paddle::instance = paddle;
}

Initialisation::~Initialisation()
{
  GetFrame().Destroy();
  SetFrame(std::shared_ptr<FrameBase>());

  Paddle::instance.reset();

  CloseHandle(g_hCustomRomF8);
  g_hCustomRomF8 = INVALID_HANDLE_VALUE;
  CloseHandle(g_hCustomRom);
  g_hCustomRom = INVALID_HANDLE_VALUE;
}

LoggerContext::LoggerContext(const bool log)
{
  if (log)
  {
    LogInit();
  }
}

LoggerContext::~LoggerContext()
{
  LogDone();
}

RegistryContext::RegistryContext(const std::shared_ptr<Registry> & registry)
{
  Registry::instance = registry;
}

RegistryContext::~RegistryContext()
{
  Registry::instance.reset();
}

void InitialiseEmulator()
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

  GetVideo().SetVidHD(false);
  LoadConfiguration(true);
  SetCurrentCLK6502();
  GetAppleWindowTitle();
  GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);

  DSInit();
  SpkrInitialize();

  MemInitialize();

  CardManager & cardManager = GetCardMgr();
  cardManager.Reset(true);

  Snapshot_Startup();

  DebugInitialize();
}

void DestroyEmulator()
{
  CardManager & cardManager = GetCardMgr();
  cardManager.Destroy();

  Snapshot_Shutdown();
  MemDestroy();
  SpkrDestroy();
  DSUninit();
  CpuDestroy();
  DebugDestroy();
  RiffFinishWriteFile();
}
