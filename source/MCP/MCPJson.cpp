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
 */

#include "StdAfx.h"

#include "MCP/MCPJson.h"

#include "StrFormat.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace MCP
{
	static const Json g_null;

	Json Json::Boolean(bool value)
	{
		Json json;
		json.m_type = TYPE_BOOL;
		json.m_bool = value;
		return json;
	}

	Json Json::Number(double value)
	{
		Json json;
		json.m_type = TYPE_NUMBER;
		json.m_number = value;
		return json;
	}

	Json Json::Integer(long long value)
	{
		return Number(static_cast<double>(value));
	}

	Json Json::Array()
	{
		Json json;
		json.m_type = TYPE_ARRAY;
		return json;
	}

	Json Json::Object()
	{
		Json json;
		json.m_type = TYPE_OBJECT;
		return json;
	}

	bool Json::AsBool(bool fallback) const
	{
		if (m_type == TYPE_BOOL)
			return m_bool;
		if (m_type == TYPE_NUMBER)
			return m_number != 0.0;
		return fallback;
	}

	double Json::AsNumber(double fallback) const
	{
		if (m_type == TYPE_NUMBER)
			return m_number;
		if (m_type == TYPE_BOOL)
			return m_bool ? 1.0 : 0.0;
		return fallback;
	}

	long long Json::AsInteger(long long fallback) const
	{
		if (m_type == TYPE_NUMBER)
			return static_cast<long long>(m_number);
		if (m_type == TYPE_BOOL)
			return m_bool ? 1 : 0;
		return fallback;
	}

	std::string Json::AsString(const std::string& fallback) const
	{
		if (m_type == TYPE_STRING)
			return m_string;
		return fallback;
	}

	bool Json::Has(const std::string& key) const
	{
		return m_type == TYPE_OBJECT && m_object.find(key) != m_object.end();
	}

	const Json& Json::operator[](const std::string& key) const
	{
		if (m_type != TYPE_OBJECT)
			return g_null;

		const std::map<std::string, Json>::const_iterator it = m_object.find(key);
		return (it == m_object.end()) ? g_null : it->second;
	}

	Json& Json::operator[](const std::string& key)
	{
		if (m_type != TYPE_OBJECT)
		{
			m_type = TYPE_OBJECT;
			m_object.clear();
			m_keyOrder.clear();
		}

		if (m_object.find(key) == m_object.end())
			m_keyOrder.push_back(key);

		return m_object[key];
	}

	size_t Json::Size() const
	{
		if (m_type == TYPE_ARRAY)
			return m_array.size();
		if (m_type == TYPE_OBJECT)
			return m_object.size();
		return 0;
	}

	const Json& Json::At(size_t index) const
	{
		if (m_type != TYPE_ARRAY || index >= m_array.size())
			return g_null;
		return m_array[index];
	}

	void Json::Append(const Json& value)
	{
		if (m_type != TYPE_ARRAY)
		{
			m_type = TYPE_ARRAY;
			m_array.clear();
		}
		m_array.push_back(value);
	}

	//===========================================================================

	void JsonEscape(const std::string& text, std::string& out)
	{
		out += '"';

		for (size_t i = 0; i < text.size(); i++)
		{
			const uint8_t ch = static_cast<uint8_t>(text[i]);

			switch (ch)
			{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (ch < 0x20)
				{
					out += StrFormat("\\u%04X", ch);
				}
				else if (ch < 0x80)
				{
					out += static_cast<char>(ch);
				}
				else
				{
					// The Apple II character set has 7 bits. A character with a
					// larger value comes from a file name. Write it as an escape
					// sequence. This prevents incorrect UTF-8 in the output.
					out += StrFormat("\\u%04X", ch);
				}
				break;
			}
		}

		out += '"';
	}

	void Json::Serialise(std::string& out) const
	{
		switch (m_type)
		{
		case TYPE_NULL:
			out += "null";
			break;

		case TYPE_BOOL:
			out += m_bool ? "true" : "false";
			break;

		case TYPE_NUMBER:
			if (m_number == floor(m_number) && fabs(m_number) < 9.0e15)
				out += StrFormat("%lld", static_cast<long long>(m_number));
			else
				out += StrFormat("%.17g", m_number);
			break;

		case TYPE_STRING:
			JsonEscape(m_string, out);
			break;

		case TYPE_ARRAY:
			out += '[';
			for (size_t i = 0; i < m_array.size(); i++)
			{
				if (i)
					out += ',';
				m_array[i].Serialise(out);
			}
			out += ']';
			break;

		case TYPE_OBJECT:
		{
			out += '{';
			bool first = true;
			for (size_t i = 0; i < m_keyOrder.size(); i++)
			{
				const std::map<std::string, Json>::const_iterator it = m_object.find(m_keyOrder[i]);
				if (it == m_object.end())
					continue;

				if (!first)
					out += ',';
				first = false;

				JsonEscape(it->first, out);
				out += ':';
				it->second.Serialise(out);
			}
			out += '}';
			break;
		}
		}
	}

	std::string Json::Serialise() const
	{
		std::string out;
		Serialise(out);
		return out;
	}

	//===========================================================================

	namespace
	{
		class Parser
		{
		public:
			Parser(const std::string& text) : m_text(text), m_pos(0) {}

			bool ParseValue(Json& out);
			const std::string& GetError() const { return m_error; }

		private:
			void SkipWhitespace();
			bool Fail(const char* message);
			bool ParseString(std::string& out);
			bool ParseNumber(Json& out);
			bool ParseLiteral(const char* literal, const Json& value, Json& out);
			bool ParseArray(Json& out);
			bool ParseObject(Json& out);
			void AppendUtf8(unsigned int codePoint, std::string& out);

			const std::string& m_text;
			size_t m_pos;
			std::string m_error;
		};

		void Parser::SkipWhitespace()
		{
			while (m_pos < m_text.size())
			{
				const char ch = m_text[m_pos];
				if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
					m_pos++;
				else
					break;
			}
		}

		bool Parser::Fail(const char* message)
		{
			m_error = StrFormat("%s at offset %u", message, static_cast<unsigned int>(m_pos));
			return false;
		}

		void Parser::AppendUtf8(unsigned int codePoint, std::string& out)
		{
			if (codePoint < 0x80)
			{
				out += static_cast<char>(codePoint);
			}
			else if (codePoint < 0x800)
			{
				out += static_cast<char>(0xC0 | (codePoint >> 6));
				out += static_cast<char>(0x80 | (codePoint & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xE0 | (codePoint >> 12));
				out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (codePoint & 0x3F));
			}
		}

		bool Parser::ParseString(std::string& out)
		{
			if (m_pos >= m_text.size() || m_text[m_pos] != '"')
				return Fail("expected a string");

			m_pos++;
			out.clear();

			while (m_pos < m_text.size())
			{
				const char ch = m_text[m_pos++];

				if (ch == '"')
					return true;

				if (ch != '\\')
				{
					out += ch;
					continue;
				}

				if (m_pos >= m_text.size())
					return Fail("unterminated escape");

				const char escape = m_text[m_pos++];
				switch (escape)
				{
				case '"':  out += '"'; break;
				case '\\': out += '\\'; break;
				case '/':  out += '/'; break;
				case 'b':  out += '\b'; break;
				case 'f':  out += '\f'; break;
				case 'n':  out += '\n'; break;
				case 'r':  out += '\r'; break;
				case 't':  out += '\t'; break;
				case 'u':
				{
					if (m_pos + 4 > m_text.size())
						return Fail("truncated \\u escape");

					unsigned int codePoint = 0;
					for (int i = 0; i < 4; i++)
					{
						const char digit = m_text[m_pos++];
						codePoint <<= 4;
						if (digit >= '0' && digit <= '9')
							codePoint |= digit - '0';
						else if (digit >= 'a' && digit <= 'f')
							codePoint |= digit - 'a' + 10;
						else if (digit >= 'A' && digit <= 'F')
							codePoint |= digit - 'A' + 10;
						else
							return Fail("bad hex digit in \\u escape");
					}

					// A surrogate pair comes as two escape sequences. Put the two
					// parts together. Then a file name with an emoji stays correct.
					if (codePoint >= 0xD800 && codePoint <= 0xDBFF &&
						m_pos + 6 <= m_text.size() && m_text[m_pos] == '\\' && m_text[m_pos + 1] == 'u')
					{
						unsigned int low = 0;
						bool valid = true;
						for (int i = 0; i < 4; i++)
						{
							const char digit = m_text[m_pos + 2 + i];
							low <<= 4;
							if (digit >= '0' && digit <= '9')
								low |= digit - '0';
							else if (digit >= 'a' && digit <= 'f')
								low |= digit - 'a' + 10;
							else if (digit >= 'A' && digit <= 'F')
								low |= digit - 'A' + 10;
							else
								valid = false;
						}

						if (valid && low >= 0xDC00 && low <= 0xDFFF)
						{
							m_pos += 6;
							const unsigned int combined = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
							out += static_cast<char>(0xF0 | (combined >> 18));
							out += static_cast<char>(0x80 | ((combined >> 12) & 0x3F));
							out += static_cast<char>(0x80 | ((combined >> 6) & 0x3F));
							out += static_cast<char>(0x80 | (combined & 0x3F));
							break;
						}
					}

					AppendUtf8(codePoint, out);
					break;
				}
				default:
					return Fail("unknown escape");
				}
			}

			return Fail("unterminated string");
		}

		bool Parser::ParseNumber(Json& out)
		{
			const char* start = m_text.c_str() + m_pos;
			char* end = NULL;
			const double value = strtod(start, &end);

			if (end == start)
				return Fail("expected a number");

			m_pos += static_cast<size_t>(end - start);
			out = Json::Number(value);
			return true;
		}

		bool Parser::ParseLiteral(const char* literal, const Json& value, Json& out)
		{
			const size_t length = strlen(literal);
			if (m_text.compare(m_pos, length, literal) != 0)
				return Fail("expected a literal");

			m_pos += length;
			out = value;
			return true;
		}

		bool Parser::ParseArray(Json& out)
		{
			m_pos++;  // past '['
			out = Json::Array();

			SkipWhitespace();
			if (m_pos < m_text.size() && m_text[m_pos] == ']')
			{
				m_pos++;
				return true;
			}

			while (true)
			{
				Json element;
				if (!ParseValue(element))
					return false;
				out.Append(element);

				SkipWhitespace();
				if (m_pos >= m_text.size())
					return Fail("unterminated array");

				if (m_text[m_pos] == ',')
				{
					m_pos++;
					continue;
				}

				if (m_text[m_pos] == ']')
				{
					m_pos++;
					return true;
				}

				return Fail("expected , or ] in array");
			}
		}

		bool Parser::ParseObject(Json& out)
		{
			m_pos++;  // past '{'
			out = Json::Object();

			SkipWhitespace();
			if (m_pos < m_text.size() && m_text[m_pos] == '}')
			{
				m_pos++;
				return true;
			}

			while (true)
			{
				SkipWhitespace();

				std::string key;
				if (!ParseString(key))
					return false;

				SkipWhitespace();
				if (m_pos >= m_text.size() || m_text[m_pos] != ':')
					return Fail("expected : after object key");
				m_pos++;

				Json value;
				if (!ParseValue(value))
					return false;
				out[key] = value;

				SkipWhitespace();
				if (m_pos >= m_text.size())
					return Fail("unterminated object");

				if (m_text[m_pos] == ',')
				{
					m_pos++;
					continue;
				}

				if (m_text[m_pos] == '}')
				{
					m_pos++;
					return true;
				}

				return Fail("expected , or } in object");
			}
		}

		bool Parser::ParseValue(Json& out)
		{
			SkipWhitespace();

			if (m_pos >= m_text.size())
				return Fail("unexpected end of input");

			switch (m_text[m_pos])
			{
			case '{': return ParseObject(out);
			case '[': return ParseArray(out);
			case '"':
			{
				std::string text;
				if (!ParseString(text))
					return false;
				out = Json(text);
				return true;
			}
			case 't': return ParseLiteral("true", Json::Boolean(true), out);
			case 'f': return ParseLiteral("false", Json::Boolean(false), out);
			case 'n': return ParseLiteral("null", Json(), out);
			default:  return ParseNumber(out);
			}
		}
	}

	bool Json::Parse(const std::string& text, Json& out, std::string& error)
	{
		Parser parser(text);

		if (!parser.ParseValue(out))
		{
			error = parser.GetError();
			return false;
		}

		return true;
	}
}
