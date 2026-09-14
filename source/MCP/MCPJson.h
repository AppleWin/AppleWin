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

/* Description: The MCP server's JSON data type
 *
 * Author: Copyright (C) 2026 Robert Baruch
 *
 * AppleWin has the zlib and the libyaml libraries, but it has no JSON library.
 * A new library makes the build more complex. Therefore this file contains the
 * JSON code for JSON-RPC. MCP messages are simple. They contain only numbers,
 * strings, arrays and objects.
 */

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace MCP
{
	class Json
	{
	public:
		enum Type_e { TYPE_NULL, TYPE_BOOL, TYPE_NUMBER, TYPE_STRING, TYPE_ARRAY, TYPE_OBJECT };

		Json() : m_type(TYPE_NULL), m_bool(false), m_number(0.0) {}
		Json(const char* value) : m_type(TYPE_STRING), m_bool(false), m_number(0.0), m_string(value ? value : "") {}
		Json(const std::string& value) : m_type(TYPE_STRING), m_bool(false), m_number(0.0), m_string(value) {}

		// These functions make a value of a specified type. With them, Json(0)
		// cannot become a bool by accident, and Json(true) cannot become the
		// number 1.
		static Json Boolean(bool value);
		static Json Number(double value);
		static Json Integer(long long value);
		static Json Array();
		static Json Object();

		Type_e GetType() const { return m_type; }
		bool IsNull() const { return m_type == TYPE_NULL; }
		bool IsObject() const { return m_type == TYPE_OBJECT; }
		bool IsArray() const { return m_type == TYPE_ARRAY; }
		bool IsString() const { return m_type == TYPE_STRING; }
		bool IsNumber() const { return m_type == TYPE_NUMBER; }
		bool IsBool() const { return m_type == TYPE_BOOL; }

		// Each function returns the fallback value if the type is different. A
		// client can send a tool argument with an incorrect type. This is a
		// client error to report. It is not a reason to stop the program.
		bool AsBool(bool fallback = false) const;
		double AsNumber(double fallback = 0.0) const;
		long long AsInteger(long long fallback = 0) const;
		std::string AsString(const std::string& fallback = std::string()) const;

		// Object functions. The const function returns a null value if the object
		// does not contain the key.
		bool Has(const std::string& key) const;
		const Json& operator[](const std::string& key) const;
		Json& operator[](const std::string& key);

		// Array functions.
		size_t Size() const;
		const Json& At(size_t index) const;
		void Append(const Json& value);

		std::string Serialise() const;
		void Serialise(std::string& out) const;

		// This function returns false and writes the error text if the input is
		// not correct JSON.
		static bool Parse(const std::string& text, Json& out, std::string& error);

	private:
		Type_e m_type;
		bool m_bool;
		double m_number;
		std::string m_string;
		std::vector<Json> m_array;
		std::map<std::string, Json> m_object;

		// The keys stay in the sequence in which the code adds them. Thus a
		// message reads in the same sequence as the code that makes it.
		std::vector<std::string> m_keyOrder;
	};

	// This function makes a JSON string literal. The result contains the two
	// quotation marks.
	void JsonEscape(const std::string& text, std::string& out);
}
