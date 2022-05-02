#include "frontends/libretro/environment.h"

#include <cstdarg>
#include <iostream>

namespace ra2
{

  std::string save_directory;

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

  void display_message(const std::string & message)
  {
    retro_message rmsg;
    rmsg.frames = 180;
    rmsg.msg = message.c_str();
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
  }

}
