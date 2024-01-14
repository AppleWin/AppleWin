#pragma once

#include "frontends/common2/commonframe.h"
#include <string>

namespace common2
{

  class GNUFrame : public CommonFrame
  {
  public:
    GNUFrame(const common2::EmulatorOptions & option);

    std::string Video_GetScreenShotFolder() const override;
    std::string getResourcePath(const std::string & filename) override;

  private:
    const std::string myHomeDir;
    const std::string myResourceFolder;
  };

}
