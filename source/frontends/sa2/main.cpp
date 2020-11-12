#include <iostream>
#include <SDL.h>
#include <memory>
#include <chrono>

#include "linux/interface.h"
#include "linux/windows/misc.h"
#include "linux/data.h"
#include "linux/paddle.h"

#include "frontends/common2/configuration.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"
#include "frontends/sa2/emulator.h"
#include "frontends/sa2/gamepad.h"
#include "frontends/sa2/sdirectsound.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "AppleWin.h"
#include "Disk.h"
#include "Mockingboard.h"
#include "SoundCore.h"
#include "Harddisk.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Video.h"
#include "NTSC.h"
#include "SaveState.h"
#include "RGBMonitor.h"
#include "Riff.h"


namespace
{
  void initialiseEmulator()
  {
    LogFileOutput("Initialisation\n");

    ImageInitialize();
    g_bFullSpeed = false;
  }

  void loadEmulator()
  {
    LoadConfiguration();
    CheckCpu();
    SetWindowTitle();
    FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

    DSInit();
    MB_Initialize();
    SpkrInitialize();

    MemInitialize();
    VideoInitialize();
    VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());

    GetCardMgr().GetDisk2CardMgr().Reset();
    HD_Reset();
  }

  void applyOptions(const EmulatorOptions & options)
  {
    if (options.log)
    {
      LogInit();
    }

    bool disksOk = true;
    if (!options.disk1.empty())
    {
      const bool ok = DoDiskInsert(SLOT6, DRIVE_1, options.disk1, options.createMissingDisks);
      disksOk = disksOk && ok;
      LogFileOutput("Init: DoDiskInsert(D1), res=%d\n", ok);
    }

    if (!options.disk2.empty())
    {
      const bool ok = DoDiskInsert(SLOT6, DRIVE_2, options.disk2, options.createMissingDisks);
      disksOk = disksOk && ok;
      LogFileOutput("Init: DoDiskInsert(D2), res=%d\n", ok);
    }

    if (!options.snapshot.empty())
    {
      setSnapshotFilename(options.snapshot);
    }

    Paddle::setSquaring(options.squaring);
  }

  void stopEmulator()
  {
    CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
    if (pMouseCard)
    {
      pMouseCard->Reset();
    }
    MemDestroy();
  }

  void uninitialiseEmulator()
  {
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

  struct Data
  {
    Emulator * emulator;
    SDL_mutex * mutex;
    bool quit;
  };

  Uint32 my_callbackfunc(Uint32 interval, void *param)
  {
    Data * data = static_cast<Data *>(param);
    if (!data->quit)
    {
      SDL_LockMutex(data->mutex);
      data->emulator->executeOneFrame();
      SDL_UnlockMutex(data->mutex);
    }
    return interval;
  }

}

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption, text, nullptr);
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

void run_sdl(int argc, const char * argv [])
{
  EmulatorOptions options;
  options.memclear = g_nMemoryClearType;
  const bool run = getEmulatorOptions(argc, argv, "SDL2", options);

  if (!run)
    return;


  if (options.log)
  {
    LogInit();
  }

  InitializeRegistry(options);

  Paddle::instance().reset(new Gamepad(0));

  g_nMemoryClearType = options.memclear;

#ifdef RIFF_SPKR
  RiffInitWriteFile("/tmp/Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
  RiffInitWriteFile("/tmp/Mockingboard.wav", 44100, 2);
#endif

  initialiseEmulator();
  loadEmulator();

  applyOptions(options);

  const int width = GetFrameBufferWidth();
  const int height = GetFrameBufferHeight();
  const int sw = GetFrameBufferBorderlessWidth();
  const int sh = GetFrameBufferBorderlessHeight();

  std::shared_ptr<SDL_Window> win(SDL_CreateWindow(g_pAppTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, sw, sh, SDL_WINDOW_SHOWN), SDL_DestroyWindow);
  if (!win)
  {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    return;
  }

  std::shared_ptr<SDL_Renderer> ren(SDL_CreateRenderer(win.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
  if (!ren)
  {
    std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
    return;
  }

  SDL_RendererInfo info;
  SDL_GetRendererInfo(ren.get(), &info);

  std::cerr << "SDL Renderer:" << info.name << std::endl;
  for (size_t i = 0; i < info.num_texture_formats; ++i)
  {
      std::cerr << SDL_GetPixelFormatName(info.texture_formats[i]) << std::endl;
  }

  const Uint32 format = SDL_PIXELFORMAT_ARGB8888;
  std::cerr << "Selected format: " << SDL_GetPixelFormatName(format) << std::endl;

  std::shared_ptr<SDL_Texture> tex(SDL_CreateTexture(ren.get(), format, SDL_TEXTUREACCESS_STATIC, width, height), SDL_DestroyTexture);

  Emulator emulator(win, ren, tex);

  std::shared_ptr<SDL_mutex> mutex(SDL_CreateMutex(), SDL_DestroyMutex);

  Data data;
  data.quit = false;
  data.mutex = mutex.get();
  data.emulator = &emulator;

  SDL_TimerID timer = SDL_AddTimer(1000 / 60, my_callbackfunc, &data);

  do
  {
    SDL_LockMutex(data.mutex);
    SDirectSound::writeAudio();
    emulator.processEvents(data.quit);
    SDL_UnlockMutex(data.mutex);
    const SDL_Rect rect = emulator.updateTexture();
    emulator.refreshVideo(rect);
  } while (!data.quit);

  SDL_RemoveTimer(timer);
  // if the following enough to make sure the timer has finished
  // and wont be called again?
  SDL_LockMutex(data.mutex);
  SDL_UnlockMutex(data.mutex);

  SDirectSound::stop();
  stopEmulator();
  uninitialiseEmulator();
}

int main(int argc, const char * argv [])
{
  //First we need to start up SDL, and make sure it went ok
  const Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO | SDL_INIT_TIMER;
  if (SDL_Init(flags) != 0)
  {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return 1;
  }

  int exit = 0;

  try
  {
    run_sdl(argc, argv);
  }
  catch (const std::exception & e)
  {
    exit = 2;
    std::cerr << e.what() << std::endl;
  }


  // this must happen BEFORE the SDL_Quit() as otherwise we have a double free (of the game controller).
  Paddle::instance().reset();
  SDL_Quit();

  return exit;
}
