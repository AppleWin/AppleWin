#include "libretro.h"

void fallback_log(enum retro_log_level level, const char *fmt, ...);
extern retro_log_callback logging;
extern retro_log_printf_t log_cb;
