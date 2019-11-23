#include "linux/windows/resources.h"
#include "../resource/resource.h"
#include "Log.h"

// forward declared
HRSRC FindResource(void *, const std::string & filename, const char *);

std::string MAKEINTRESOURCE(int x)
{
  switch (x)
  {
  case IDR_APPLE2_ROM: return "Apple2.rom";
  case IDR_APPLE2_PLUS_ROM: return "Apple2_Plus.rom";
  case IDR_APPLE2E_ROM: return "Apple2e.rom";
  case IDR_APPLE2E_ENHANCED_ROM: return "Apple2e_Enhanced.rom";
  case IDR_PRAVETS_82_ROM: return "PRAVETS82.ROM";
  case IDR_PRAVETS_8C_ROM: return "PRAVETS8C.ROM";
  case IDR_PRAVETS_8M_ROM: return "PRAVETS8M.ROM";
  case IDR_TK3000_2E_ROM: return "TK3000e.rom";

  case IDR_DISK2_FW: return "DISK2.rom";
  case IDR_SSC_FW: return "SSC.rom";
  case IDR_HDDRVR_FW: return "Hddrvr.bin";
  case IDR_PRINTDRVR_FW: return "Parallel.rom";
  case IDR_MOCKINGBOARD_D_FW: return "Mockingboard-D.rom";
  case IDR_MOUSEINTERFACE_FW: return "MouseInterface.rom";
  case IDR_THUNDERCLOCKPLUS_FW: return "ThunderClockPlus.rom";
  case IDR_TKCLOCK_FW: return "TKClock.rom";
  }

  LogFileOutput("Unknown resource %d\n", x);
  return std::string();
}

DWORD SizeofResource(void *, const HRSRC & res)
{
  return res.data.size();
}

HGLOBAL LoadResource(void *, HRSRC & res)
{
  return &res;
}

BYTE * LockResource(HGLOBAL data)
{
  HRSRC & rsrc = dynamic_cast<HRSRC &>(*data);
  return (BYTE *)rsrc.data.data();
}
