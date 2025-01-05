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

  protected:
    // pointer and size
    std::pair<const unsigned char *, unsigned int> GetResourceData(WORD id) const;

  private:
    const std::string myHomeDir;
  };

}
