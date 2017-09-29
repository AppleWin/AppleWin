#pragma once

#include "linux/interface.h"
#include <boost/property_tree/ptree.hpp>

void InitializeRegistry(const std::string & filename);

const boost::property_tree::ptree & getProperties();

void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);
