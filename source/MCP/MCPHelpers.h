#pragma once

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

#include <string>

namespace MCP
{
	// This function lowers the ASCII letters only. The bytes above 0x7F stay as
	// they are, and the current locale has no effect. Header field names and
	// tool arguments are compared this way.
	std::string ToLower(const std::string& text);

	// This function returns true if text begins with prefix. The comparison is
	// case-sensitive.
	bool StartsWith(const std::string& text, const char* prefix);

	// These functions remove spaces, tabs, carriage returns and line feeds
	// from the end of the text, or from both ends.
	std::string TrimRight(const std::string& text);
	std::string Trim(const std::string& text);
}
