#include "libretro.h"

#include <string>

namespace ra2
{

  void fallback_log(enum retro_log_level level, const char *fmt, ...);
  extern retro_log_callback logging;
  extern retro_log_printf_t log_cb;
  extern retro_input_poll_t input_poll_cb;
  extern retro_input_state_t input_state_cb;
  extern retro_environment_t environ_cb;
  extern retro_video_refresh_t video_cb;
  extern retro_audio_sample_t audio_cb;
  extern retro_audio_sample_batch_t audio_batch_cb;

  extern std::string retro_base_directory;

#define MAX_PADS 1

  void display_message(const std::string & message);

}
