/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: The MCP server's string helpers
 *
 * Author: Copyright (C) 2026 Robert Baruch
 */

#include "StdAfx.h"

#include "MCP/MCPHelpers.h"

#include <cstddef>
#include <cstring>
#include <string>

namespace MCP
{
	static const char g_whitespace[] = " \t\r\n";

	std::string ToLower(const std::string& text)
	{
		std::string lower(text);
		for (size_t i = 0; i < lower.size(); i++)
		{
			const char ch = lower[i];
			if (ch >= 'A' && ch <= 'Z')
				lower[i] = static_cast<char>(ch - 'A' + 'a');
		}
		return lower;
	}

	bool StartsWith(const std::string& text, const char* prefix)
	{
		const size_t length = strlen(prefix);
		return text.size() >= length && text.compare(0, length, prefix) == 0;
	}

	std::string TrimRight(const std::string& text)
	{
		const size_t last = text.find_last_not_of(g_whitespace);
		if (last == std::string::npos)
			return std::string();
		return text.substr(0, last + 1);
	}

	std::string Trim(const std::string& text)
	{
		const size_t first = text.find_first_not_of(g_whitespace);
		if (first == std::string::npos)
			return std::string();
		return TrimRight(text.substr(first));
	}
}
