#pragma once

#include "frontends/common2/commonframe.h"
#include <string>

namespace common2
{

  class GNUFrame : public virtual CommonFrame
  {
  public:
    GNUFrame();

    std::string Video_GetScreenShotFolder() const override;
    std::string getResourcePath(const std::string & filename) override;

  private:
    const std::string myHomeDir;
    const std::string myResourceFolder;
  };

}
