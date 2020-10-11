#pragma once

#include "linux/windows/wincompat.h"
#include <string>

struct EmulatorOptions;

void InitializeRegistry(const EmulatorOptions & options);

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);
