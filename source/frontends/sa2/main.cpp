#include <iostream>
#include <SDL.h>
#include <memory>

#include "linux/interface.h"
#include "linux/windows/misc.h"
#include "linux/data.h"
#include "frontends/common2/configuration.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
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
    ResetDefaultMachineMemTypes();

    MemInitialize();
    VideoInitialize();
    VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideoType());

    g_CardMgr.GetDisk2CardMgr().Reset();
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

  }

  void stopEmulator()
  {
    CMouseInterface* pMouseCard = g_CardMgr.GetMouseCard();
    if (pMouseCard)
    {
      pMouseCard->Reset();
    }
    MemDestroy();
  }

  void uninitialiseEmulator()
  {
    HD_Destroy();
    PrintDestroy();
    CpuDestroy();

    g_CardMgr.GetDisk2CardMgr().Destroy();
    ImageDestroy();
    LogDone();
  }

  SDL_Rect refreshTexture(const std::shared_ptr<SDL_Texture> & tex)
  {
    uint8_t * data;
    int width;
    int height;
    int sx, sy;
    int sw, sh;

    getScreenData(data, width, height, sx, sy, sw, sh);

    void * pixels;
    int pitch;
    SDL_LockTexture(tex.get(), nullptr, &pixels, &pitch);

    memcpy(pixels, data, width * height * 4);

    SDL_UnlockTexture(tex.get());

    SDL_Rect srect;
    srect.x = sx;
    srect.y = sy;
    srect.w = sw;
    srect.h = sh;

    return srect;
  }

  void renderScreen(const std::shared_ptr<SDL_Renderer> & ren, const std::shared_ptr<SDL_Texture> & tex, const SDL_Rect & srect)
  {
    SDL_RenderCopyEx(ren.get(), tex.get(), &srect, nullptr, 0.0, nullptr, SDL_FLIP_VERTICAL);
    SDL_RenderPresent(ren.get());
  }

  void cycleVideoType(const std::shared_ptr<SDL_Window> & win)
  {
    g_eVideoType++;
    if (g_eVideoType >= NUM_VIDEO_MODES)
      g_eVideoType = 0;

    SetWindowTitle();
    SDL_SetWindowTitle(win.get(), g_pAppTitle.c_str());

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();
  }

  void cycle50ScanLines(const std::shared_ptr<SDL_Window> & win)
  {
    VideoStyle_e videoStyle = GetVideoStyle();
    videoStyle = VideoStyle_e(videoStyle ^ VS_HALF_SCANLINES);

    SetVideoStyle(videoStyle);

    SetWindowTitle();
    SDL_SetWindowTitle(win.get(), g_pAppTitle.c_str());

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();
  }

  int getFPS()
  {
    SDL_DisplayMode current;

    int should_be_zero = SDL_GetCurrentDisplayMode(0, &current);

    if (should_be_zero)
    {
      throw std::string(SDL_GetError());
    }

    return current.refresh_rate;
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

BYTE SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
  return MemReadFloatingBus(uExecutedCycles);
}


// Mockingboard
void registerSoundBuffer(IDirectSoundBuffer * buffer)
{
}

void unregisterSoundBuffer(IDirectSoundBuffer * buffer)
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

  g_nMemoryClearType = options.memclear;

  initialiseEmulator();
  loadEmulator();

  applyOptions(options);

  const int width = GetFrameBufferWidth();
  const int height = GetFrameBufferHeight();
  const int sx = GetFrameBufferBorderWidth();
  const int sy = GetFrameBufferBorderHeight();
  const int sw = GetFrameBufferBorderlessWidth();
  const int sh = GetFrameBufferBorderlessHeight();

  int multiplier = 1;
  bool fullscreen = false;

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

  const Uint32 format = SDL_PIXELFORMAT_BGRA32;
  std::shared_ptr<SDL_Texture> tex(SDL_CreateTexture(ren.get(), format, SDL_TEXTUREACCESS_STREAMING, width, height), SDL_DestroyTexture);

  const int fps = getFPS();
  const DWORD uCyclesToExecute = int(g_fCurrentCLK6502 / fps);
  const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

  bool quit = false;
  do
  {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
      if (e.type == SDL_QUIT)
      {
	quit = true;
      }
      else if (e.type == SDL_KEYDOWN)
      {
	if (!e.key.repeat)
	{
	  switch (e.key.keysym.scancode)
	  {
	  case SDL_SCANCODE_F9:
	  {
	    cycleVideoType(win);
	    break;
	  }
	  case SDL_SCANCODE_F6:
	  {
	    if ((e.key.keysym.mod & KMOD_CTRL) && (e.key.keysym.mod & KMOD_SHIFT))
	    {
	      cycle50ScanLines(win);
	    }
	    else if (e.key.keysym.mod & KMOD_CTRL)
	    {
	      multiplier = multiplier == 1 ? 2 : 1;
	      SDL_SetWindowSize(win.get(), sw * multiplier, sh * multiplier);
	    }
	    else if (!(e.key.keysym.mod & KMOD_SHIFT))
	    {
	      fullscreen = !fullscreen;
	      SDL_SetWindowFullscreen(win.get(), fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	    }
	    break;
	  }
	  case SDL_SCANCODE_F5:
	  {
	    if (g_CardMgr.QuerySlot(SLOT6) == CT_Disk2)
	    {
	      dynamic_cast<Disk2InterfaceCard*>(g_CardMgr.GetObj(SLOT6))->DriveSwap();
	    }
	    break;
	  }
	  }
	}
	std::cerr << e.key.keysym.scancode << "," << e.key.keysym.sym << "," << e.key.keysym.mod << "," << bool(e.key.repeat) << ",AAA" << std::endl;
      }
    }

    const bool bVideoUpdate = true;
    const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
    g_dwCyclesThisFrame += uActualCyclesExecuted;

    g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);

    // SDL2 seems to synch with screen refresh rate so we do not need to worry about timers
    const SDL_Rect srect = refreshTexture(tex);
    renderScreen(ren, tex, srect);

    g_dwCyclesThisFrame = (g_dwCyclesThisFrame + g_dwCyclesThisFrame) % dwClksPerFrame;
  } while (!quit);

  stopEmulator();
  uninitialiseEmulator();
}

int main(int argc, const char * argv [])
{
  //First we need to start up SDL, and make sure it went ok
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
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

  SDL_Quit();

  return exit;
}
