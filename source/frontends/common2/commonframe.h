#pragma once

#include "linux/linuxframe.h"

#include "Common.h"
#include "Configuration/Config.h"

#include "frontends/common2/speed.h"
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

    void ExecuteOneFrame(const int64_t microseconds);


    void ChangeMode(const AppMode_e mode);
    void SingleStep();

    void ResetHardware();
    bool HardwareChanged() const;

    void LoadSnapshot() override;

  protected:
    virtual std::string getResourcePath(const std::string & filename) = 0;

    static std::string getBitmapFilename(const std::string & resource);

    virtual void SetFullSpeed(const bool value);
    virtual bool CanDoFullSpeed();

    void ExecuteInRunningMode(const int64_t microseconds);
    void ExecuteInDebugMode(const int64_t microseconds);
    void Execute(const DWORD uCycles);

    const bool myAllowVideoUpdate;
    Speed mySpeed;

    std::vector<BYTE> myResource;


  private:
    CConfigNeedingRestart myHardwareConfig;
  };

}
