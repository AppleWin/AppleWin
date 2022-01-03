#pragma once

#include "linux/linuxframe.h"
#include <vector>
#include <string>

namespace common2
{

  class CommonFrame : public LinuxFrame
  {
  public:
    CommonFrame();

    BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize) override;

    std::string Video_GetScreenShotFolder() override;

  protected:
    static std::string getBitmapFilename(const std::string & resource);

    const std::string myResourcePath;
    std::vector<BYTE> myResource;
  };

}
