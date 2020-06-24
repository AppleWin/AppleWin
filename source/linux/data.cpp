#include "StdAfx.h"
#include "Common.h"

#include "Log.h"
#include "CPU.h"
#include "Applewin.h"
#include "Video.h"


void SetWindowTitle()
{
  switch (g_Apple2Type)
  {
  default:
  case A2TYPE_APPLE2:			g_pAppTitle = TITLE_APPLE_2; break;
  case A2TYPE_APPLE2PLUS:		g_pAppTitle = TITLE_APPLE_2_PLUS; break;
  case A2TYPE_APPLE2JPLUS:		g_pAppTitle = TITLE_APPLE_2_JPLUS; break;
  case A2TYPE_APPLE2E:                  g_pAppTitle = TITLE_APPLE_2E; break;
  case A2TYPE_APPLE2EENHANCED:          g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break;
  case A2TYPE_PRAVETS82:		g_pAppTitle = TITLE_PRAVETS_82; break;
  case A2TYPE_PRAVETS8M:		g_pAppTitle = TITLE_PRAVETS_8M; break;
  case A2TYPE_PRAVETS8A:		g_pAppTitle = TITLE_PRAVETS_8A; break;
  case A2TYPE_TK30002E:                 g_pAppTitle = TITLE_TK3000_2E; break;
  }

  g_pAppTitle += " - ";

  if( IsVideoStyle(VS_HALF_SCANLINES) )
  {
          g_pAppTitle += " 50% ";
  }
  g_pAppTitle += g_apVideoModeDesc[ g_eVideoType ];
}

void CheckCpu()
{
  const eApple2Type apple2Type = GetApple2Type();
  const eCpuType defaultCpu = ProbeMainCpuDefault(apple2Type);
  const eCpuType mainCpu = GetMainCpu();
  if (mainCpu != defaultCpu)
  {
    LogFileOutput("Detected non standard CPU for Apple2 = %d: default = %d, actual = %d\n", apple2Type, defaultCpu, mainCpu);
  }
}

void alarm_log_too_many_alarms()
{
}
