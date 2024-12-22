#include "registryclass.h"
#include "windows.h"
#include "Log.h"

std::shared_ptr<Registry> Registry::instance;

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue)
{
  BOOL success = RegLoadString(section, key, peruser, buffer, chars);
  if (!success)
    StringCbCopy(buffer, chars, defaultValue);
  return success;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t* value, uint32_t defaultValue) {
  BOOL success = RegLoadValue(section, key, peruser, value);
  if (!success)
    *value = defaultValue;
  return success;
}

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser,
                    LPTSTR buffer, uint32_t chars)
{
  BOOL result;
  try
  {
    const std::string s = Registry::instance->getString(section, key);
    strncpy(buffer, s.c_str(), chars);
    buffer[chars - 1] = 0;
    result = TRUE;
    LogFileOutput("RegLoadString: %s - %s = %s\n", section, key, buffer);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadString: %s - %s = ??\n", section, key);
  }
  return result;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t *value)
{
  BOOL result;
  try
  {
    *value = Registry::instance->getDWord(section, key);
    result = TRUE;
    LogFileOutput("RegLoadValue: %s - %s = %d\n", section, key, *value);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadValue: %s - %s = ??\n", section, key);
  }
  return result;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value)
{
  BOOL result;
  try
  {
    *value = Registry::instance->getBool(section, key);
    result = TRUE;
    LogFileOutput("RegLoadValue: %s - %s = %d\n", section, key, *value);
  }
  catch (const std::exception & e)
  {
    result = FALSE;
    LogFileOutput("RegLoadValue: %s - %s = ??\n", section, key);
  }
  return result;
}

void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer)
{
  Registry::instance->putString(section, key, buffer);
  LogFileOutput("RegSaveString: %s - %s = %s\n", section, key, buffer.c_str());
}

void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, uint32_t value)
{
  Registry::instance->putDWord(section, key, value);
  LogFileOutput("RegSaveValue: %s - %s = %d\n", section, key, value);
}
