#include <iostream>
#include <SDL.h>
#include <memory>

#include "linux/interface.h"
#include "linux/windows/misc.h"
#include "linux/data.h"
#include "frontends/ncurses/configuration.h"

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


namespace
{
  void initialiseEmulator()
  {
    g_fh = fopen("/tmp/applewin.txt", "w");
    setbuf(g_fh, nullptr);

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

    g_CardMgr.GetDisk2CardMgr().Reset();
    HD_Reset();
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
    fclose(g_fh);
    g_fh = nullptr;
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

void run_sdl()
{
  InitializeRegistry("applen.conf");

  initialiseEmulator();
  loadEmulator();

  const int width = GetFrameBufferWidth();
  const int height = GetFrameBufferHeight();
  const int sx = GetFrameBufferBorderWidth();
  const int sy = GetFrameBufferBorderHeight();
  const int sw = GetFrameBufferBorderlessWidth();
  const int sh = GetFrameBufferBorderlessHeight();

  int multiplier = 1;

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
	      break;
	    }
	  }
	}
	std::cerr << e.key.keysym.scancode << "," << e.key.keysym.sym << "," << e.key.keysym.mod << "," << bool(e.key.repeat) << ",AAA" << std::endl;
      }
    }

    const bool bVideoUpdate = true;
    DWORD uCyclesToExecute = 1000;
    const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
    g_dwCyclesThisFrame += uActualCyclesExecuted;

    g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);

    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
    if (g_dwCyclesThisFrame >= dwClksPerFrame)
    {
      if (g_dwCyclesThisFrame >= dwClksPerFrame)
      {
	const SDL_Rect srect = refreshTexture(tex);
	renderScreen(ren, tex, srect);
      }
      g_dwCyclesThisFrame -= dwClksPerFrame;
    }
  } while (!quit);

  stopEmulator();
  uninitialiseEmulator();
}

int main(int, char**)
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
    run_sdl();
  }
  catch (const std::exception & e)
  {
    exit = 2;
    std::cerr << e.what() << std::endl;
  }

  SDL_Quit();

  return exit;
}
