#include "StdAfx.h"

#include <iostream>

#include "Common.h"
#include "Configuration/IPropertySheet.h"
#include "CPU.h"

#include <unistd.h>


eApple2Type GetApple2Type(void)
{
	return g_Apple2Type;
}

void SetApple2Type(eApple2Type type)
{
	g_Apple2Type = type;
	SetMainCpuDefault(type);
}

void DeleteCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void InitializeCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void EnterCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

void LeaveCriticalSection(CRITICAL_SECTION * criticalSection)
{
}

bool SetCurrentImageDir(const std::string & dir ) {
  chdir(dir.c_str());
  return true;
}

void OutputDebugString(const char * str)
{
  std::cerr << str << std::endl;
}

UINT IPropertySheet::GetTheFreezesF8Rom(void)
{
  return 0;
}

void IPropertySheet::ConfigSaveApple2Type(eApple2Type apple2Type)
{
}

HWND GetDesktopWindow()
{
  return NULL;
}

void ExitProcess(int status)
{
  exit(status);
}

void alarm_log_too_many_alarms()
{
}
