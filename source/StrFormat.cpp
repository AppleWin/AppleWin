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
	// VS2013 or before, _vsnprintf() cannot return required buffer size.
	int len = _vsnprintf(buf, _countof(buf), format, va);
	if (len >= 0 && size_t(len) < _countof(buf))
	{
		return std::string(buf, size_t(len)); // No overflow.
	}
	else
	{
		// Overflow, need growing buffer.
		std::string s;
		size_t bufsz = _countof(buf);
		// Allow buffer to grow to at most 2^ngrow times sizeof(buf).
		int const ngrow = 4;
		for (int i = 0; i < ngrow; ++i)
		{
			bufsz *= 2;
			char *pbuf = new char[bufsz];
			len = _vsnprintf(pbuf, bufsz, format, va);
			bool done = false;
			if (len >= 0 && size_t(len) < bufsz)
			{
				s.assign(pbuf, size_t(len));
				done = true;
			}
			else if ((i+1) >= ngrow)
			{
				// Truncated at 2^ngrow times sizeof(buf).
				s.assign(pbuf, bufsz - 1);
				done = true;
			}
			delete[] pbuf;
			if (done)
				break;
		}
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
