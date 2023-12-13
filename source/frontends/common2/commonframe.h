#pragma once

#include "linux/linuxframe.h"
#include "Common.h"
#include "Configuration/Config.h"
#include "speed.h"
#include <vector>
#include <string>

namespace common2
{
  struct EmulatorOptions;

  class CommonFrame : public LinuxFrame
  {
  public:
    CommonFrame(const EmulatorOptions & options);

    void Begin() override;

    BYTE* GetResource(WORD id, LPCSTR lpType, DWORD expectedSize) override;

    virtual void ResetSpeed();

    void ExecuteOneFrame(const uint64_t microseconds);

    void ChangeMode(const AppMode_e mode);
    void SingleStep();

    void ResetHardware();
    bool HardwareChanged() const;

  protected:
    virtual std::string getResourcePath(const std::string & filename) = 0;

    static std::string getBitmapFilename(const std::string & resource);

    virtual void SetFullSpeed(const bool value);
    virtual bool CanDoFullSpeed();

    void ExecuteInRunningMode(const uint64_t microseconds);
    void ExecuteInDebugMode(const uint64_t microseconds);
    void Execute(const DWORD uCycles);

    std::vector<BYTE> myResource;

    Speed mySpeed;

  private:
    CConfigNeedingRestart myHardwareConfig;
  };

}
