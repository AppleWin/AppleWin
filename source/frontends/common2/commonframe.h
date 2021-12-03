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

    void Initialize(bool resetVideoState) override;
    void Destroy() override;
    void Restart() override;

    BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize) override;

  protected:
    static std::string getBitmapFilename(const std::string & resource);

    const std::string myResourcePath;
    std::vector<BYTE> myResource;
  };

}
