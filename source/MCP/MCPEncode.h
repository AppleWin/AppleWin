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

/* Description: The MCP server's PNG and base64 encoders
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * MCP sends an image as a base64 string with a MIME type. Therefore a screenshot
 * must leave the emulator as compressed data in memory, not as a file on disk.
 * AppleWin already uses zlib for disk images. zlib supplies the two parts that
 * PNG makes necessary: the deflate stream and the CRC.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MCP
{
	// This function encodes a 24-bit RGB image. The first line of the input is
	// the top line of the image. The function returns false only if zlib fails.
	bool EncodePng(const uint8_t* rgb, unsigned int width, unsigned int height, std::vector<uint8_t>& out);

	std::string EncodeBase64(const uint8_t* data, size_t length);

	// This function returns false if the text contains a character that is not in
	// the base64 alphabet.
	bool DecodeBase64(const std::string& text, std::vector<uint8_t>& out);
}
