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
retro_input_poll_t input_poll_cb;
retro_input_state_t input_state_cb;

retro_environment_t environ_cb;
retro_video_refresh_t video_cb;
retro_audio_sample_t audio_cb;
retro_audio_sample_batch_t audio_batch_cb;

std::string retro_base_directory;

unsigned int retro_devices[RETRO_DEVICES] = {};
