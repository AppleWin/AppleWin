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
#include "StrFormat.h"

namespace _ini {
	//===========================================================================
	bool RegLoadString(LPCTSTR section, LPCTSTR key, bool /*peruser*/, LPTSTR buffer, uint32_t chars)
	{
		uint32_t n = GetPrivateProfileString(section, key, NULL, buffer, chars, g_sConfigFile.c_str());
		return n > 0;
	}

	//===========================================================================
	void RegSaveString(LPCTSTR section, LPCTSTR key, bool /*peruser*/, const std::string& buffer)
	{
		bool updated = !!WritePrivateProfileString(section, key, buffer.c_str(), g_sConfigFile.c_str());
		_ASSERT(updated || GetLastError() == 0);
	}

	//===========================================================================
	void RegDeleteString(LPCTSTR section, bool /*peruser*/)
	{
	    bool updated = !!WritePrivateProfileString(section, NULL, NULL, g_sConfigFile.c_str());
		_ASSERT(updated || GetLastError() == 0);
	}
}

//===========================================================================
bool RegLoadString (LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars)
{
	if (!g_sConfigFile.empty())
		return _ini::RegLoadString(section, key, peruser, buffer, chars);

	std::string fullkeyname = std::string("Software\\AppleWin\\CurrentVersion\\") + section;

	bool success = false;
	HKEY keyhandle;
	LSTATUS status = RegOpenKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname.c_str(),
		0,
		KEY_READ,
		&keyhandle);
	if (status == ERROR_SUCCESS)
	{
		DWORD type;
		DWORD size = chars;
		status = RegQueryValueEx(keyhandle, key, NULL, &type, (LPBYTE)buffer, &size);
		if (status == 0 && size != 0)
			success = true;
	}

	RegCloseKey(keyhandle);

	return success;
}

//===========================================================================
bool RegLoadString (LPCTSTR section, LPCTSTR key, bool peruser, LPTSTR buffer, uint32_t chars, LPCTSTR defaultValue)
{
	bool success = RegLoadString(section, key, peruser, buffer, chars);
	if (!success)
		StringCbCopy(buffer, chars, defaultValue);
	return success;
}

//===========================================================================
bool RegLoadValue (LPCTSTR section, LPCTSTR key, bool peruser, uint32_t* value) {
	char buffer[32];
	if (!RegLoadString(section, key, peruser, buffer, 32))
	{
		return false;
	}

	*value = (uint32_t)atoi(buffer);
	return true;
}

//===========================================================================
bool RegLoadValue (LPCTSTR section, LPCTSTR key, bool peruser, uint32_t* value, uint32_t defaultValue) {
	bool success = RegLoadValue(section, key, peruser, value);
	if (!success)
		*value = defaultValue;
	return success;
}

//===========================================================================
void RegSaveString (LPCTSTR section, LPCTSTR key, bool peruser, const std::string & buffer) {
	if (!g_sConfigFile.empty())
		return _ini::RegSaveString(section, key, peruser, buffer);

	std::string fullkeyname = std::string("Software\\AppleWin\\CurrentVersion\\") + section;

	HKEY  keyhandle;
	DWORD disposition;
	LSTATUS status = RegCreateKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname.c_str(),
		0,
		NULL,
		REG_OPTION_NON_VOLATILE,
		KEY_READ | KEY_WRITE,
		(LPSECURITY_ATTRIBUTES)NULL,
		&keyhandle,
		&disposition);
	if (status == ERROR_SUCCESS)
	{
		RegSetValueEx(
			keyhandle,
			key,
			0,
			REG_SZ,
			(CONST LPBYTE)buffer.c_str(),
			(uint32_t)((buffer.size() + 1) * sizeof(char)));
		RegCloseKey(keyhandle);
	}
}

//===========================================================================
static inline std::string RegGetSlotSection(UINT slot)
{
	if (slot == GAME_IO_CONNECTOR)
		return std::string(REG_CONFIG_GAME_IO_CONNECTOR);

	return (slot == SLOT_AUX)
		? std::string(REG_CONFIG_SLOT_AUX)
		: (std::string(REG_CONFIG_SLOT) + (char)('0' + slot));
}

std::string RegGetConfigSlotSection(UINT slot)
{
	return std::string(REG_CONFIG "\\") + RegGetSlotSection(slot);
}

void RegDeleteConfigSlotSection(UINT slot)
{
	constexpr bool peruser = true;

	if (!g_sConfigFile.empty())
	{
		std::string section = RegGetConfigSlotSection(slot);
		return _ini::RegDeleteString(section.c_str(), peruser);
	}

	std::string fullkeyname = std::string("Software\\AppleWin\\CurrentVersion\\") + REG_CONFIG;

	HKEY keyhandle;
	LSTATUS status = RegOpenKeyEx(
		(peruser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE),
		fullkeyname.c_str(),
		0,
		KEY_READ,
		&keyhandle);
	if (status == ERROR_SUCCESS)
	{
		std::string section = RegGetSlotSection(slot);
		LSTATUS status2 = RegDeleteKey(keyhandle, section.c_str());
		if (status2 != ERROR_SUCCESS && status2 != ERROR_FILE_NOT_FOUND)
			_ASSERT(0);
	}

	RegCloseKey(keyhandle);
}

void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type)
{
	_ASSERT(slot != GAME_IO_CONNECTOR);

	RegDeleteConfigSlotSection(slot);

	std::string regSection;
	regSection = RegGetConfigSlotSection(slot);

	RegSaveValue(regSection.c_str(), REGVALUE_CARD_TYPE, true, type);
}

void RegSetConfigGameIOConnectorNewDongleType(UINT slot, DONGLETYPE type)
{
	_ASSERT(slot == GAME_IO_CONNECTOR);
	if (slot != GAME_IO_CONNECTOR)
		return;

	RegDeleteConfigSlotSection(slot);

	std::string regSection;
	regSection = RegGetConfigSlotSection(slot);

	RegSaveValue(regSection.c_str(), REGVALUE_GAME_IO_TYPE, true, type);
}
