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
  const std::shared_ptr<Registry> & registry,
  const std::shared_ptr<FrameBase> & frame,
  const std::shared_ptr<Paddle> & paddle
  )
{
  Registry::instance = registry;
  SetFrame(frame);
  Paddle::instance = paddle;

  frame->Initialize();
}

Initialisation::~Initialisation()
{
  GetFrame().Destroy();
  SetFrame(std::shared_ptr<FrameBase>());

  Paddle::instance.reset();
  Registry::instance.reset();
}

Logger::Logger(const bool log)
{
  if (log)
  {
    LogInit();
  }
}

Logger::~Logger()
{
  LogDone();
}
