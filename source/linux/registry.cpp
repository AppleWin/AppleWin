#include "linux/windows/wincompat.h"
#include "linux/windows/stringcb.h"
#include "linux/interface.h"

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars, LPCTSTR defaultValue)
{
	BOOL success = RegLoadString(section, key, peruser, buffer, chars);
	if (!success)
		StringCbCopy(buffer, chars, defaultValue);
	return success;
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value, DWORD defaultValue) {
	BOOL success = RegLoadValue(section, key, peruser, value);
	if (!success)
		*value = defaultValue;
	return success;
}
