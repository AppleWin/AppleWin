#include "frontends/retro/environment.h"

#include <cstdarg>
#include <iostream>


void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
  (void)level;
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
}

retro_log_callback logging;
retro_log_printf_t log_cb = fallback_log;
