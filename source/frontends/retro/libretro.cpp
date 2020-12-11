#include "libretro.h"
#include <string>

#include "StdAfx.h"
#include "Frame.h"
#include "Video.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "Mockingboard.h"
#include "SoundCore.h"
#include "Harddisk.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "NTSC.h"
#include "SaveState.h"
#include "RGBMonitor.h"
#include "Riff.h"
#include "Utilities.h"

#include <linux/version.h>
#include <linux/videobuffer.h>

#include "frontends/common2/programoptions.h"
#include "frontends/common2/configuration.h"
#include "frontends/common2/speed.h"
#include "frontends/retro/environment.h"

namespace
{
  bool use_audio_cb;
  float last_aspect;
  float last_sample_rate;
  std::string retro_base_directory;

  retro_video_refresh_t video_cb;
  retro_audio_sample_t audio_cb;
  retro_audio_sample_batch_t audio_batch_cb;
  retro_input_poll_t input_poll_cb;
  retro_input_state_t input_state_cb;
  retro_rumble_interface rumble;

  retro_environment_t environ_cb;

  Speed speed(true);

  void initialiseEmulator()
  {
#ifdef RIFF_SPKR
    RiffInitWriteFile("/tmp/Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
    RiffInitWriteFile("/tmp/Mockingboard.wav", 44100, 2);
#endif

    g_nAppMode = MODE_RUNNING;
    LogFileOutput("Initialisation\n");

    ImageInitialize();
    g_bFullSpeed = false;

    LoadConfiguration();
    SetCurrentCLK6502();
    GetAppleWindowTitle();

    DSInit();
    MB_Initialize();
    SpkrInitialize();

    MemInitialize();
    VideoBufferInitialize();
    VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());

    GetCardMgr().GetDisk2CardMgr().Reset();
    HD_Reset();
    Snapshot_Startup();
  }

  void uninitialiseEmulator()
  {
    Snapshot_Shutdown();
    CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
    if (pMouseCard)
    {
      pMouseCard->Reset();
    }
    VideoBufferDestroy();
    MemDestroy();

    SpkrDestroy();
    MB_Destroy();
    DSUninit();

    HD_Destroy();
    PrintDestroy();
    CpuDestroy();

    GetCardMgr().GetDisk2CardMgr().Destroy();
    ImageDestroy();
    LogDone();
    RiffFinishWriteFile();
  }

}

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s: %s - %s\n", __FUNCTION__, caption, text);
  return IDOK;
}

void FrameDrawDiskLEDS(HDC x)
{
}

void FrameDrawDiskStatus(HDC x)
{
}

void FrameRefreshStatus(int x, bool)
{
}

void retro_init(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  const char *dir = NULL;
  if (environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir) && dir)
  {
    retro_base_directory = dir;
  }

  EmulatorOptions options;
  options.memclear = g_nMemoryClearType;
  options.log = true;
  options.useQtIni = true;

  if (options.log)
  {
    LogInit();
  }

  InitializeRegistry(options);
  initialiseEmulator();
}

void retro_deinit(void)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  uninitialiseEmulator();
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
}

void retro_get_system_info(retro_system_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  static std::string version = getVersion();

  memset(info, 0, sizeof(*info));
  info->library_name     = "AppleWin";
  info->library_version  = version.c_str();
  info->need_fullpath    = true;
  info->valid_extensions = "dsk";
}


void retro_get_system_av_info(retro_system_av_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  float aspect                = 0.0f;
  float sampling_rate         = 30000.0f;

  info->geometry.base_width   = GetFrameBufferWidth();
  info->geometry.base_height  = GetFrameBufferHeight();
  info->geometry.max_width    = GetFrameBufferWidth();
  info->geometry.max_height   = GetFrameBufferHeight();
  info->geometry.aspect_ratio = aspect;

  last_aspect                 = aspect;
  last_sample_rate            = sampling_rate;
}

void retro_set_environment(retro_environment_t cb)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  environ_cb = cb;

  if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
    log_cb = logging.log;
  else
    log_cb = fallback_log;

  static const retro_controller_description controllers[] =
    {
     { "Nintendo DS", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 0) },
    };

  static const retro_controller_info ports[] =
    {
     { controllers, 1 },
     { NULL, 0 },
    };

  cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
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
  if (g_nAppMode == MODE_RUNNING)
  {
    const size_t cyclesToExecute = speed.getCyclesTillNext(16);

    const bool bVideoUpdate = true;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

    const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);

    g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
    GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedCycles);
    MB_PeriodicUpdate(executedCycles);
    SpkrUpdate(executedCycles);
  }

  //  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  const size_t pitch = GetFrameBufferWidth() * 4;
  input_poll_cb();
  video_cb(g_pFramebufferbits, GetFrameBufferWidth(), GetFrameBufferHeight(), pitch);
}

bool retro_load_game(const retro_game_info *info)
{
  log_cb(RETRO_LOG_INFO, "RA2: %s\n", __FUNCTION__);
  retro_input_descriptor desc[] =
    {
     { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
     { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
     { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
     { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
     { 0 },
    };

  environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
  {
    log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
    return false;
  }

  const bool ok = DoDiskInsert(SLOT6, DRIVE_1, info->path);
  log_cb(RETRO_LOG_INFO, "Game path: %s:%d\n", info->path, ok);

  return true;
}

void retro_unload_game(void)
{
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
