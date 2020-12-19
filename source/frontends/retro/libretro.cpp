#include "libretro.h"
#include <memory>
#include <cstring>

#include "StdAfx.h"
#include "Frame.h"

#include "linux/version.h"
#include "linux/paddle.h"

#include "frontends/retro/game.h"
#include "frontends/retro/environment.h"
#include "frontends/retro/joypad.h"

namespace
{
  std::unique_ptr<Game> game;
}

void retro_init(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  const char *dir = NULL;
  if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
  {
    retro_base_directory = dir;
  }
}

void retro_deinit(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

unsigned retro_api_version(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  log_cb(RETRO_LOG_INFO, "Plugging device %u into port %u.\n", device, port);

  if (port < RETRO_DEVICES)
    retro_devices[port] = device;
}

void retro_get_system_info(retro_system_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  static std::string version = getVersion();

  memset(info, 0, sizeof(*info));
  info->library_name     = "AppleWin";
  info->library_version  = version.c_str();
  info->need_fullpath    = true;
  info->valid_extensions = "bin|do|dsk|nib|po|gz|woz|zip|2mg|2img|iie|apl|hdv";
}


void retro_get_system_av_info(retro_system_av_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);

  info->geometry.base_width   = GetFrameBufferBorderlessWidth();
  info->geometry.base_height  = GetFrameBufferBorderlessHeight();
  info->geometry.max_width    = GetFrameBufferBorderlessWidth();
  info->geometry.max_height   = GetFrameBufferBorderlessHeight();
  info->geometry.aspect_ratio = 0;

  info->timing.fps            = 60;
  info->timing.sample_rate    = 0;
}

void retro_set_environment(retro_environment_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  environ_cb = cb;

  if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
    log_cb = logging.log;
  else
    log_cb = fallback_log;

   static const struct retro_controller_description controllers[] = {
      { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
   };

   static const struct retro_controller_info ports[] = {
      { controllers, 1 },
      { NULL, 0 },
   };

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

   //  retro_keyboard_callback callback = {&Game::keyboardCallback};
   //  cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &callback);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  video_cb = cb;
}

void retro_run(void)
{
  game->processInputEvents();
  game->executeOneFrame();
  game->drawVideoBuffer();
}

bool retro_load_game(const retro_game_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);

  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
  {
    log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
    return false;
  }

  try
  {
    game.reset(new Game);
    const bool ok = game->loadGame(info->path);

    log_cb(RETRO_LOG_INFO, "Game path: %s:%d\n", info->path, ok);

    Paddle::instance().reset(new Joypad);
    Paddle::setSquaring(false);

    return ok;
  }
  catch (const std::exception & e)
  {
    log_cb(RETRO_LOG_INFO, "Exception: %s\n", e.what());
  }
  catch (const std::string & s)
  {
    log_cb(RETRO_LOG_INFO, "Exception: %s\n", s.c_str());
  }

  return false;
}

void retro_unload_game(void)
{
  game.reset();
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

unsigned retro_get_region(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return RETRO_REGION_NTSC;
}

void retro_reset(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return false;
}

size_t retro_serialize_size(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return 0;
}

bool retro_serialize(void *data, size_t size)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return false;
}

bool retro_unserialize(const void *data, size_t size)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return false;
}

void retro_cheat_reset(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

void *retro_get_memory_data(unsigned id)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return nullptr;
}

size_t retro_get_memory_size(unsigned id)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return 0;
}
