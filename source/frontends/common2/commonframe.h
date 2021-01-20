#pragma once

#include "linux/linuxframe.h"
#include <vector>
#include <string>

class CommonFrame : public LinuxFrame
{
public:
  CommonFrame();

  virtual void Destroy();

  virtual BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize);

protected:
  static std::string getBitmapFilename(const std::string & resource);

  const std::string myResourcePath;
  std::vector<BYTE> myResource;
};
