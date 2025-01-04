#include "StdAfx.h"

#include "StrFormat.h"

#include <cassert>

std::string StrFormat(const char* format, ...)
{
	va_list va;
	va_start(va, format);
	std::string s = StrFormatV(format, va);
	va_end(va);
	return s;
}

std::string StrFormatV(const char* format, va_list va)
{
	size_t const bufsz_base = 2048; // Big enough for most cases.
	char buf[bufsz_base];

	// Need to call va_copy() so va can be used potentially for a second time.
	// glibc on Linux *certainly* needs this while MSVC is fine without it though.
	va_list va_copied;
	va_copy(va_copied, va);
	int len = vsnprintf(buf, sizeof(buf), format, va_copied);
	va_end(va_copied);

	if (len < 0)
	{
		return std::string(); // Error.
	}
	else if (size_t(len) < sizeof(buf))
	{
		return std::string(buf, size_t(len)); // No overflow.
	}
	else
	{
		// Overflow, need bigger buffer.
		std::string s(size_t(len), '\0');
		len = vsnprintf(&(*s.begin()), s.length() + 1, format, va);
		assert(size_t(len) == s.length());
		return s;
	}
}
