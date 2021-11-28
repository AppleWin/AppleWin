#pragma once

#include "wincompat.h"
#include <string>
#include <map>
#include <memory>
#include <cstring>

class Registry
{
public:
  virtual ~Registry();

  static std::shared_ptr<Registry> instance;

  virtual std::string getString(const std::string & section, const std::string & key) const = 0;
  virtual DWORD getDWord(const std::string & section, const std::string & key) const = 0;
  virtual bool getBool(const std::string & section, const std::string & key) const = 0;

  virtual void putString(const std::string & section, const std::string & key, const std::string & value) = 0;
  virtual void putDWord(const std::string & section, const std::string & key, const DWORD value) = 0;

  virtual std::map<std::string, std::map<std::string, std::string>> getAllValues() const = 0;

};

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value);
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value);
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer);
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value);
