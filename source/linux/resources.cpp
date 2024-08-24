#include "StdAfx.h"
#include "linux/resources.h"
#include "../resource/resource.h"

namespace
{
  const std::map<WORD, std::string> resources =
    {
     {IDR_APPLE2_ROM, "Apple2.rom"},
     {IDR_APPLE2_PLUS_ROM, "Apple2_Plus.rom"},
     {IDR_APPLE2_VIDEO_ROM, "Apple2_Video.rom"},
     {IDR_APPLE2E_ENHANCED_VIDEO_ROM, "Apple2e_Enhanced_Video.rom"},
     {IDR_APPLE2_JPLUS_ROM, "Apple2_JPlus.rom"},
     {IDR_APPLE2E_ROM, "Apple2e.rom"},
     {IDR_APPLE2E_ENHANCED_ROM, "Apple2e_Enhanced.rom"},
     {IDR_PRAVETS_82_ROM, "PRAVETS82.ROM"},
     {IDR_PRAVETS_8C_ROM, "PRAVETS8C.ROM"},
     {IDR_PRAVETS_8M_ROM, "PRAVETS8M.ROM"},
     {IDR_TK3000_2E_ROM, "TK3000e.rom"},
     {IDR_BASE_64A_ROM, "Base64A.rom"},

     {IDR_APPLE2_JPLUS_VIDEO_ROM, "Apple2_JPlus_Video.rom"},
     {IDR_DISK2_16SECTOR_FW, "DISK2.rom"},
     {IDR_DISK2_13SECTOR_FW, "DISK2-13sector.rom"},
     {IDR_SSC_FW, "SSC.rom"},
     {IDR_HDDRVR_FW, "Hddrvr.bin"},
     {IDR_PRINTDRVR_FW, "Parallel.rom"},
     {IDR_MOCKINGBOARD_D_FW, "Mockingboard-D.rom"},
     {IDR_MOUSEINTERFACE_FW, "MouseInterface.rom"},
     {IDR_THUNDERCLOCKPLUS_FW, "ThunderClockPlus.rom"},
     {IDR_TKCLOCK_FW, "TKClock.rom"},
     {IDR_BASE64A_VIDEO_ROM, "Base64A_German_Video.rom"},
     {IDR_HDDRVR_V2_FW, "Hddrvr-v2.bin"},
     {IDR_HDC_SMARTPORT_FW, "HDC-SmartPort.bin"},
    };
}

const std::string & getResourceName(WORD x)
{
  const auto it = resources.find(x);
  if (it == resources.end())
  {
    throw std::runtime_error("Missing resource: " + std::to_string(x));
  }
  return it->second;
}
