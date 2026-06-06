#include "StdAfx.h"
#include "registryclass.h"
#include "Registry.h"
#include "Log.h"

std::shared_ptr<Registry> Registry::instance;

bool RegLoadString(LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue)
{
    bool success = RegLoadString(section, key, peruser, buffer, chars);
    if (!success)
        StringCbCopy(buffer, chars, defaultValue);
    return success;
}

bool RegLoadValue(LPCTSTR section, LPCTSTR key, bool peruser, uint32_t *value, uint32_t defaultValue)
{
    bool success = RegLoadValue(section, key, peruser, value);
    if (!success)
        *value = defaultValue;
    return success;
}

bool RegLoadString(LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars)
{
    bool result;
    try
    {
        const std::string s = Registry::instance->getString(section, key);
        strncpy(buffer, s.c_str(), chars);
        buffer[chars - 1] = 0;
        result = TRUE;
        LogFileOutput("RegLoadString: %s - %s = %s\n", section, key, buffer);
    }
    catch (const std::exception &e)
    {
        result = FALSE;
        LogFileOutput("RegLoadString: %s - %s = ??\n", section, key);
    }
    return result;
}

bool RegLoadValue(LPCTSTR section, LPCTSTR key, bool peruser, uint32_t *value)
{
    bool result;
    try
    {
        *value = Registry::instance->getDWord(section, key);
        result = TRUE;
        LogFileOutput("RegLoadValue: %s - %s = %d\n", section, key, *value);
    }
    catch (const std::exception &e)
    {
        result = FALSE;
        LogFileOutput("RegLoadValue: %s - %s = ??\n", section, key);
    }
    return result;
}

void RegSaveString(LPCTSTR section, LPCTSTR key, bool peruser, const std::string &buffer)
{
    Registry::instance->putString(section, key, buffer);
    LogFileOutput("RegSaveString: %s - %s = %s\n", section, key, buffer.c_str());
}

void RegSaveValue(LPCTSTR section, LPCTSTR key, bool peruser, uint32_t value)
{
    Registry::instance->putDWord(section, key, value);
    LogFileOutput("RegSaveValue: %s - %s = %d\n", section, key, value);
}
