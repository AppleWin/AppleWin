#pragma once

#include <string>
#include <cstdarg>

#ifdef _MSC_VER
#define ATTRIBUTE_FORMAT_PRINTF(a, b)
#else
#define ATTRIBUTE_FORMAT_PRINTF(a, b) __attribute__((format(printf, a, b)))
#endif

std::string StrFormat(const char* format, ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);
std::string StrFormatV(const char* format, va_list va);
