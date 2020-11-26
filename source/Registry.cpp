/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Registry module
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Registry.h"
#include "CmdLine.h"

namespace _ini {
	//===========================================================================
	BOOL RegLoadString(LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars)
	{
		DWORD n = GetPrivateProfileString(section, key, NULL, buffer, chars, g_sConfigFile.c_str());
		return n > 0;
	}

	//===========================================================================
	void RegSaveString(LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string& buffer)
	{
		BOOL updated = WritePrivateProfileString(section, key, buffer.c_str(), g_sConfigFile.c_str());
		_ASSERT(updated || GetLastError() == 0);
	}
}

//===========================================================================
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars)
{
	if (!g_sConfigFile.empty())
		return _ini::RegLoadString(section, key, peruser, buffer, chars);

	TCHAR fullkeyname[256];
	StringCbPrintf(fullkeyname, 256, TEXT("Software\\AppleWin\\CurrentVersion\\%s"), section);

	BOOL success = FALSE;
	HKEY keyhandle;
	LSTATUS status = RegOpenKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname,
		0,
		KEY_READ,
		&keyhandle);
	if (status == 0)
	{
		DWORD type;
		DWORD size = chars;
		status = RegQueryValueEx(keyhandle, key, NULL, &type, (LPBYTE)buffer, &size);
		if (status == 0 && size != 0)
			success = TRUE;
	}

	RegCloseKey(keyhandle);

	return success;
}

//===========================================================================
BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars, LPCTSTR defaultValue)
{
	BOOL success = RegLoadString(section, key, peruser, buffer, chars);
	if (!success)
		StringCbCopy(buffer, chars, defaultValue);
	return success;
}

//===========================================================================
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value) {
	TCHAR buffer[32];
	if (!RegLoadString(section, key, peruser, buffer, 32))
	{
		return FALSE;
	}

	*value = (DWORD)_ttoi(buffer);
	return TRUE;
}

//===========================================================================
BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD* value, DWORD defaultValue) {
	BOOL success = RegLoadValue(section, key, peruser, value);
	if (!success)
		*value = defaultValue;
	return success;
}

//===========================================================================
void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, const std::string & buffer) {
	if (!g_sConfigFile.empty())
		return _ini::RegSaveString(section, key, peruser, buffer);

	TCHAR fullkeyname[256];
	StringCbPrintf(fullkeyname, 256, TEXT("Software\\AppleWin\\CurrentVersion\\%s"), section);

	HKEY  keyhandle;
	DWORD disposition;
	LSTATUS status = RegCreateKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname,
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE,
		(LPSECURITY_ATTRIBUTES)NULL,
		&keyhandle,
		&disposition);
	if (status == 0)
	{
		RegSetValueEx(
			keyhandle,
			key,
			0,
			REG_SZ,
			(CONST LPBYTE)buffer.c_str(),
			(buffer.size() + 1) * sizeof(TCHAR));
		RegCloseKey(keyhandle);
	}
}

//===========================================================================
void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value) {
	TCHAR buffer[32] = TEXT("");
	StringCbPrintf(buffer, 32, "%d", value);
	RegSaveString(section, key, peruser, buffer);
}

