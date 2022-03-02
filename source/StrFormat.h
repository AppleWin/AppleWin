#pragma once

#include <cstdarg>
#include <string>

#if defined(_MSC_VER) && _MSC_VER < 1600
#include <basetsd.h>
typedef UINT8 uint8_t;
#else
#include <cstdint>
#endif

#ifdef _MSC_VER
#define ATTRIBUTE_FORMAT_PRINTF(a, b)
#else
#define ATTRIBUTE_FORMAT_PRINTF(a, b) __attribute__((format(printf, a, b)))
#endif

std::string StrFormat(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
std::string StrFormatV(const char* format, va_list va);

inline std::string& StrAppendByteAsHex(std::string& s, uint8_t n)
{
	const char szHex[] = "0123456789ABCDEF";
	s += szHex[(n >> 4) & 0x0f];
	s += szHex[n & 0x0f];
	return s;
}
