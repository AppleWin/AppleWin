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

  int tot1 = 0;
  int tot2 = 0;
  int tot3 = 0;
  int tot4 = 0;

  const auto start = std::chrono::steady_clock::now();
  auto t0 = start;

  bool quit = false;
  do
  {
    SDirectSound::writeAudio();
    emulator.processEvents(quit);
    const auto t1 = std::chrono::steady_clock::now();
    tot1 += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    emulator.executeOneFrame();
    const auto t2 = std::chrono::steady_clock::now();
    tot2 += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    const SDL_Rect rect = emulator.updateTexture();
    const auto t3 = std::chrono::steady_clock::now();
    tot3 += std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
    emulator.refreshVideo(rect);
    const auto t4 = std::chrono::steady_clock::now();
    tot4 += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
    t0 = t4;
  } while (!quit);

  const auto end = std::chrono::steady_clock::now();
  const int tot = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  std::cerr << "Total: " << tot << std::endl;
  std::cerr << "1: " << tot1 << std::endl;
  std::cerr << "2: " << tot2 << std::endl;
  std::cerr << "3: " << tot3 << std::endl;
  std::cerr << "4: " << tot4 << std::endl;
  std::cerr << "T: " << tot1 + tot2 + tot3 + tot4 << std::endl;

  SDirectSound::stop();
  stopEmulator();
  uninitialiseEmulator();
}

int main(int argc, const char * argv [])
{
  //First we need to start up SDL, and make sure it went ok
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0)
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
