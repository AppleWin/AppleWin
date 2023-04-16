#include "StdAfx.h"
#include "frontends/common2/commoncontext.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "linux/linuxframe.h"

namespace common2
{

  CommonInitialisation::CommonInitialisation(
    const std::shared_ptr<LinuxFrame> & frame,
    const std::shared_ptr<Paddle> & paddle,
    const EmulatorOptions & options)
  : Initialisation(frame, paddle)
  {
    applyOptions(options);
    myFrame->Begin();
    setSnapshotFilename(options.snapshotFilename);
    if (options.loadSnapshot)
    {
      myFrame->LoadSnapshot();
    }

  }
  CommonInitialisation::~CommonInitialisation()
  {
    myFrame->End();
  }

}