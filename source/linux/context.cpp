#include "StdAfx.h"

#include "linux/context.h"
#include "linux/linuxframe.h"
#include "linux/registryclass.h"
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
  const std::shared_ptr<LinuxFrame> & frame,
  const std::shared_ptr<Paddle> & paddle)
: myFrame(frame)
{
  SetFrame(myFrame);
  Paddle::instance = paddle;
}

Initialisation::~Initialisation()
{
  myFrame->Destroy();
  SetFrame(std::shared_ptr<FrameBase>());

  Paddle::instance.reset();

  RiffFinishWriteFile();

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
}
