#include "StdAfx.h"

#include "linux/context.h"
#include "linux/linuxframe.h"
#include "linux/registry.h"
#include "linux/paddle.h"
#include "linux/duplicates/PropertySheet.h"

#include "Interface.h"
#include "Log.h"

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
