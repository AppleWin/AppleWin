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
#if defined(_MSC_VER) && _MSC_VER < 1900
#pragma warning(push)
#pragma warning(disable: 4996) // warning _vsnprintf() is unsafe.
	// VS2013 or before, _vsnprintf() cannot return required buffer size in case of overflow.
	int len = _vsnprintf(buf, _countof(buf), format, va);
	if (len >= 0 && size_t(len) <= _countof(buf))
	{
		// _vsnprintf() can fill up the full buffer without nul termination.
		return std::string(buf, size_t(len)); // No overflow.
	}
	else
	{
		// Overflow, need bigger buffer.
		len = _vsnprintf(NULL, 0, format, va); // Get required buffer size.
		std::string s(size_t(len), '\0');
		len = _vsnprintf(&(*s.begin()), s.length() + 1, format, va);
		assert(size_t(len) == s.length());
		return s;
	}
#pragma warning(pop)
#else
	int len = vsnprintf(buf, _countof(buf), format, va);
	if (len < 0)
	{
		return std::string(); // Error.
	}
	else if (size_t(len) < _countof(buf))
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
#endif
}
