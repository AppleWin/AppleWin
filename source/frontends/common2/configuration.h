#pragma once

#include "linux/windows/wincompat.h"
#include <boost/property_tree/ptree.hpp>
#include <string>

void InitializeRegistry(const std::string & filename, const bool saveOnExit);

const boost::property_tree::ptree & getProperties();

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);
