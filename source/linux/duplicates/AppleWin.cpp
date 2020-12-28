#include "StdAfx.h"

#include "Interface.h"
#include "linux/duplicates/PropertySheet.h"
#include "linux/linuxframe.h"
#include "linux/linuxvideo.h"

IPropertySheet& GetPropertySheet()
{
  static CPropertySheet sg_PropertySheet;
  return sg_PropertySheet;
}

FrameBase& GetFrame()
{
  static LinuxFrame sg_LinuxFrame;
  return sg_LinuxFrame;
}

Video& GetVideo()
{
  static LinuxVideo sg_LinuxVideo;
  return sg_LinuxVideo;
}
