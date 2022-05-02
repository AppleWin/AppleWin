#include "libretro.h"
#include <memory>
#include <cstring>

#include "StdAfx.h"
#include "Common.h"
#include "Video.h"
#include "Interface.h"
#include "Memory.h"
#include "Utilities.h"
#include "Debugger/DebugDefs.h"

#include "linux/version.h"
#include "linux/paddle.h"

#include "frontends/libretro/game.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/joypad.h"
#include "frontends/libretro/analog.h"
#include "frontends/libretro/mouse.h"
#include "frontends/libretro/serialisation.h"

namespace
{

  std::unique_ptr<ra2::Game> ourGame;

  bool endsWith(const std::string & value, const std::string & ending)
  {
    if (ending.size() > value.size())
    {
      return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
  }

  bool retro_set_eject_state(bool ejected)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, ejected);
    return ourGame->getDiskControl().setEjectedState(ejected);
  }

  bool retro_get_eject_state()
  {
    return ourGame->getDiskControl().getEjectedState();
  }

  unsigned retro_get_image_index()
  {
    return ourGame->getDiskControl().getImageIndex();
  }

  bool retro_set_image_index(unsigned index)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, index);
    return ourGame->getDiskControl().setImageIndex(index);
  }

  unsigned retro_get_num_images()
  {
    return ourGame->getDiskControl().getNumImages();
  }

  bool retro_replace_image_index(unsigned index, const struct retro_game_info *info)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%s)\n", __FUNCTION__, info->path);
    if (info->path)
    {
      return ourGame->getDiskControl().replaceImageIndex(index, info->path);
    }
    else
    {
      return ourGame->getDiskControl().removeImageIndex(index);
    }
  }

  bool retro_add_image_index()
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
    return ourGame->getDiskControl().addImageIndex();
  }

  bool retro_set_initial_image(unsigned index, const char *path)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d) = %s\n", __FUNCTION__, index, path);
    ra2::DiskControl::setInitialPath(index, path);
    return true;
  }

  bool retro_get_image_path(unsigned index, char *path, size_t len)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, index);
    return ourGame->getDiskControl().getImagePath(index, path, len);
  }

  bool retro_get_image_label(unsigned index, char *label, size_t len)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, index);
    return ourGame->getDiskControl().getImageLabel(index, label, len);
  }

}

void retro_init(void)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

void retro_deinit(void)
{
  ourGame.reset();
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s, Plugging device %u into port %u.\n", __FUNCTION__, device, port);
  if (port == 0)
  {
    ra2::Game::ourInputDevices[port] = device;

    switch (device)
    {
    case RETRO_DEVICE_NONE:
      Paddle::instance.reset();
      break;
    case RETRO_DEVICE_JOYPAD:
      Paddle::instance.reset(new ra2::Joypad(device));
      Paddle::setSquaring(false);
      break;
    case RETRO_DEVICE_ANALOG:
      Paddle::instance.reset(new ra2::Analog(device));
      Paddle::setSquaring(true);
      break;
    case RETRO_DEVICE_MOUSE:
      Paddle::instance.reset(new ra2::Mouse(device, &ourGame));
      Paddle::setSquaring(false);
      break;
    default:
      break;
    }
  }
}

void retro_get_system_info(retro_system_info *info)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  static std::string version = getVersion();

  memset(info, 0, sizeof(*info));
  info->library_name     = "AppleWin";
  info->library_version  = version.c_str();
  info->need_fullpath    = true;
  info->valid_extensions = "bin|do|dsk|nib|po|gz|woz|zip|2mg|2img|iie|apl|hdv|yaml|m3u";
}

void retro_get_system_av_info(retro_system_av_info *info)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);

  Video & video = GetVideo();

  info->geometry.base_width   = video.GetFrameBufferBorderlessWidth();
  info->geometry.base_height  = video.GetFrameBufferBorderlessHeight();
  info->geometry.max_width    = video.GetFrameBufferBorderlessWidth();
  info->geometry.max_height   = video.GetFrameBufferBorderlessHeight();
  info->geometry.aspect_ratio = 0;

  info->timing.fps            = ra2::Game::FPS;
  info->timing.sample_rate    = SPKR_SAMPLE_RATE;
}

void retro_set_environment(retro_environment_t cb)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  ra2::environ_cb = cb;

  if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &ra2::logging))
    ra2::log_cb = ra2::logging.log;
  else
    ra2::log_cb = ra2::fallback_log;

  static const struct retro_controller_description controllers[] =
    {
     { "Standard Joypad", RETRO_DEVICE_JOYPAD },
     { "Analog Joypad", RETRO_DEVICE_ANALOG },
     { "Mouse", RETRO_DEVICE_MOUSE },
    };

  static const struct retro_controller_info ports[] =
    {
     { controllers, sizeof(controllers) / sizeof(controllers[0]) },
     { nullptr, 0 }
    };

  cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);

  retro_keyboard_callback keyboardCallback = {&ra2::Game::keyboardCallback};
  cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboardCallback);

  retro_audio_buffer_status_callback audioCallback = {&ra2::bufferStatusCallback};
  cb(RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK, &audioCallback);

  retro_frame_time_callback timeCallback = {&ra2::Game::frameTimeCallback, 1000000 / ra2::Game::FPS};
  cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &timeCallback);

  // see retro_get_memory_data() below
  bool achievements = true;
  cb(RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS, &achievements);

  const char * saveDirectory = nullptr;
  if (cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &saveDirectory))
  {
    ra2::save_directory = saveDirectory;
  }

  unsigned dciVersion = 0;
  if (cb(RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION, &dciVersion) && (dciVersion >= 1))
  {
    retro_disk_control_ext_callback diskControlExtCallback = {
      &retro_set_eject_state, &retro_get_eject_state,
      &retro_get_image_index, &retro_set_image_index,
      &retro_get_num_images, &retro_replace_image_index,
      &retro_add_image_index, nullptr,
      &retro_get_image_path, &retro_get_image_label
    };
    // intentionally skip retro_set_initial_image
    // as we do always want to restart with the first disk of a playlist
    // which (normally) is the only bootable floppy of a game
    cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &diskControlExtCallback);
  }
  else
  {
    retro_disk_control_callback diskControlCallback = {
      &retro_set_eject_state, &retro_get_eject_state,
      &retro_get_image_index, &retro_set_image_index,
      &retro_get_num_images, &retro_replace_image_index,
      &retro_add_image_index
    };
    cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &diskControlCallback);
  }

  ra2::SetupRetroVariables();
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
  ra2::audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
  ra2::audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
  ra2::input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
  ra2::input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
  ra2::video_cb = cb;
}

void retro_run(void)
{
  ourGame->processInputEvents();
  ourGame->executeOneFrame();
  GetFrame().VideoPresentScreen();
  const size_t ms = (1000 + 60 - 1) / 60; // round up
  ra2::writeAudio(ms);
}

bool retro_load_game(const retro_game_info *info)
{
  ourGame.reset();
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);

  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!ra2::environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
  {
    ra2::log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
    return false;
  }

  try
  {
    std::unique_ptr<ra2::Game> game(new ra2::Game());

    const std::string snapshotEnding = ".aws.yaml";
    const std::string playlistEnding = ".m3u";

    bool ok;

    if (info->path && *info->path)
    {
      const std::string gamePath = info->path;
      if (endsWith(gamePath, snapshotEnding))
      {
        ok = game->loadSnapshot(gamePath);
      }
      else if (endsWith(gamePath, playlistEnding))
      {
        ok = game->getDiskControl().insertPlaylist(gamePath);
      }
      else
      {
        ok = game->getDiskControl().insertDisk(gamePath);
      }
    }
    else
    {
      ok = false;
    }

    ra2::log_cb(RETRO_LOG_INFO, "Game path: %s -> %d\n", info->path, ok);

    if (ok)
    {
      ra2::display_message("Enable Game Focus Mode for better keyboard handling");
      std::swap(ourGame, game);
    }

    return ok;
  }
  catch (const std::exception & e)
  {
    ra2::log_cb(RETRO_LOG_INFO, "Exception: %s\n", e.what());
  }

  return false;
}

void retro_unload_game(void)
{
  ourGame.reset();
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

void retro_reset(void)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  ResetMachineState();
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  return false;
}

size_t retro_serialize_size(void)
{
  try
  {
    const size_t size = ra2::RetroSerialisation::getSize();
    return size;
  }
  catch(const std::exception& e)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s - %s\n", __FUNCTION__, e.what());
    return 0;
  }
}

bool retro_serialize(void *data, size_t size)
{
  try
  {
    ra2::RetroSerialisation::serialise(data, size, ourGame->getDiskControl());
    return true;
  }
  catch(const std::exception& e)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s - %s\n", __FUNCTION__, e.what());
    return false;
  }
}

bool retro_unserialize(const void *data, size_t size)
{
  try
  {
    ra2::RetroSerialisation::deserialise(data, size, ourGame->getDiskControl());
    return true;
  }
  catch(const std::exception& e)
  {
    ra2::log_cb(RETRO_LOG_INFO, "RA2: %s - %s\n", __FUNCTION__, e.what());
    return false;
  }
}

void retro_cheat_reset(void)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
}

void *retro_get_memory_data(unsigned id)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, id);
  switch (id & RETRO_MEMORY_MASK)
  {
  case RETRO_MEMORY_SYSTEM_RAM:
    return MemGetBankPtr(0);
  default:
    return nullptr;
  };
}

size_t retro_get_memory_size(unsigned id)
{
  ra2::log_cb(RETRO_LOG_INFO, "RA2: %s (%d)\n", __FUNCTION__, id);
  switch (id & RETRO_MEMORY_MASK)
  {
  case RETRO_MEMORY_SYSTEM_RAM:
    return _6502_MEM_LEN;
  default:
    return 0;
  };
}
