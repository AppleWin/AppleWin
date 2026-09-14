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
 */

#include "StdAfx.h"

#include "MCP/MCPEncode.h"

#include <zlib.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace MCP
{
	namespace
	{
		void AppendBigEndian32(std::vector<uint8_t>& out, unsigned int value)
		{
			out.push_back(static_cast<uint8_t>(value >> 24));
			out.push_back(static_cast<uint8_t>(value >> 16));
			out.push_back(static_cast<uint8_t>(value >> 8));
			out.push_back(static_cast<uint8_t>(value));
		}

		// A PNG chunk has four parts: the length, the type, the data, and a
		// CRC. The CRC covers the type and the data, but not the length.
		void AppendChunk(std::vector<uint8_t>& out, const char type[4], const uint8_t* data, size_t length)
		{
			AppendBigEndian32(out, static_cast<unsigned int>(length));

			const size_t crcStart = out.size();
			out.insert(out.end(), type, type + 4);
			if (length)
				out.insert(out.end(), data, data + length);

			const uLong crc = crc32(crc32(0L, Z_NULL, 0), &out[crcStart], static_cast<uInt>(out.size() - crcStart));
			AppendBigEndian32(out, static_cast<unsigned int>(crc));
		}
	}

	bool EncodePng(const uint8_t* rgb, unsigned int width, unsigned int height, std::vector<uint8_t>& out)
	{
		out.clear();

		if (!rgb || !width || !height)
			return false;

		// Each PNG line starts with a filter byte. Filter 0 keeps the line
		// unchanged. The file becomes larger, but the code stays simple. An
		// Apple II screen is small, thus the increase in size is small.
		const size_t stride = static_cast<size_t>(width) * 3;
		std::vector<uint8_t> raw(height * (stride + 1));

		for (unsigned int y = 0; y < height; y++)
		{
			uint8_t* dst = &raw[y * (stride + 1)];
			*dst++ = 0;
			memcpy(dst, rgb + y * stride, stride);
		}

		uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
		std::vector<uint8_t> compressed(compressedSize);

		if (compress2(&compressed[0], &compressedSize, &raw[0], static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK)
			return false;

		static const uint8_t signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
		out.insert(out.end(), signature, signature + sizeof(signature));

		uint8_t header[13];
		header[0] = static_cast<uint8_t>(width >> 24);
		header[1] = static_cast<uint8_t>(width >> 16);
		header[2] = static_cast<uint8_t>(width >> 8);
		header[3] = static_cast<uint8_t>(width);
		header[4] = static_cast<uint8_t>(height >> 24);
		header[5] = static_cast<uint8_t>(height >> 16);
		header[6] = static_cast<uint8_t>(height >> 8);
		header[7] = static_cast<uint8_t>(height);
		header[8] = 8;    // bits per channel
		header[9] = 2;    // colour type 2 is truecolour RGB
		header[10] = 0;   // deflate
		header[11] = 0;   // adaptive filtering
		header[12] = 0;   // no interlace

		AppendChunk(out, "IHDR", header, sizeof(header));
		AppendChunk(out, "IDAT", &compressed[0], compressedSize);
		AppendChunk(out, "IEND", NULL, 0);

		return true;
	}

	//===========================================================================

	static const char g_base64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string EncodeBase64(const uint8_t* data, size_t length)
	{
		std::string out;
		out.reserve(((length + 2) / 3) * 4);

		size_t i = 0;
		while (i + 3 <= length)
		{
			const unsigned int group = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
			out += g_base64Alphabet[(group >> 18) & 0x3F];
			out += g_base64Alphabet[(group >> 12) & 0x3F];
			out += g_base64Alphabet[(group >> 6) & 0x3F];
			out += g_base64Alphabet[group & 0x3F];
			i += 3;
		}

		if (i + 1 == length)
		{
			const unsigned int group = data[i] << 16;
			out += g_base64Alphabet[(group >> 18) & 0x3F];
			out += g_base64Alphabet[(group >> 12) & 0x3F];
			out += "==";
		}
		else if (i + 2 == length)
		{
			const unsigned int group = (data[i] << 16) | (data[i + 1] << 8);
			out += g_base64Alphabet[(group >> 18) & 0x3F];
			out += g_base64Alphabet[(group >> 12) & 0x3F];
			out += g_base64Alphabet[(group >> 6) & 0x3F];
			out += '=';
		}

		return out;
	}

	bool DecodeBase64(const std::string& text, std::vector<uint8_t>& out)
	{
		out.clear();

		int accumulator = 0;
		int bits = 0;

		for (size_t i = 0; i < text.size(); i++)
		{
			const char ch = text[i];

			if (ch == '=' || ch == '\r' || ch == '\n' || ch == ' ' || ch == '\t')
				continue;

			int value;
			if (ch >= 'A' && ch <= 'Z')
				value = ch - 'A';
			else if (ch >= 'a' && ch <= 'z')
				value = ch - 'a' + 26;
			else if (ch >= '0' && ch <= '9')
				value = ch - '0' + 52;
			else if (ch == '+')
				value = 62;
			else if (ch == '/')
				value = 63;
			else
				return false;

			accumulator = (accumulator << 6) | value;
			bits += 6;

			if (bits >= 8)
			{
				bits -= 8;
				out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
			}
		}

		return true;
	}
}
