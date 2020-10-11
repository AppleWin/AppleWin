#include "frontends/sa2/emulator.h"

#include <iostream>

#include "linux/data.h"
#include "linux/paddle.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "Applewin.h"
#include "Disk.h"
#include "CPU.h"
#include "Frame.h"
#include "Video.h"
#include "NTSC.h"

namespace
{

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
      throw std::runtime_error(SDL_GetError());
    }

    return current.refresh_rate;
  }

}


Emulator::Emulator(
  const std::shared_ptr<SDL_Window> & window,
  const std::shared_ptr<SDL_Renderer> & renderer,
  const std::shared_ptr<SDL_Texture> & texture
)
  : myWindow(window)
  , myRenderer(renderer)
  , myTexture(texture)
  , myFPS(getFPS())
  , myMultiplier(1)
  , myFullscreen(false)
{
}

void Emulator::executeOneFrame()
{
  const DWORD uCyclesToExecute = int(g_fCurrentCLK6502 / myFPS);
  const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
  const bool bVideoUpdate = true;
  const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
  g_dwCyclesThisFrame += uActualCyclesExecuted;

  g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);

  // SDL2 seems to synch with screen refresh rate so we do not need to worry about timers
  const SDL_Rect srect = refreshTexture(myTexture);
  renderScreen(myRenderer, myTexture, srect);

  g_dwCyclesThisFrame = (g_dwCyclesThisFrame + g_dwCyclesThisFrame) % dwClksPerFrame;
}

void Emulator::processEvents(bool & quit)
{
  SDL_Event e;
  while (SDL_PollEvent(&e) != 0)
  {
    switch (e.type)
    {
    case SDL_QUIT:
    {
      quit = true;
      break;
    }
    case SDL_KEYDOWN:
    {
      processKeyDown(e, quit);
      break;
    }
    case SDL_KEYUP:
    {
      processKeyUp(e);
      break;
    }
    }
  }
}

void Emulator::processKeyDown(const SDL_Event & e, bool & quit)
{
  if (!e.key.repeat)
  {
    switch (e.key.keysym.scancode)
    {
    case SDL_SCANCODE_F9:
    {
      cycleVideoType(myWindow);
      break;
    }
    case SDL_SCANCODE_F6:
    {
      if ((e.key.keysym.mod & KMOD_CTRL) && (e.key.keysym.mod & KMOD_SHIFT))
      {
	cycle50ScanLines(myWindow);
      }
      else if (e.key.keysym.mod & KMOD_CTRL)
      {
	myMultiplier = myMultiplier == 1 ? 2 : 1;
	const int sw = GetFrameBufferBorderlessWidth();
	const int sh = GetFrameBufferBorderlessHeight();
	SDL_SetWindowSize(myWindow.get(), sw * myMultiplier, sh * myMultiplier);
      }
      else if (!(e.key.keysym.mod & KMOD_SHIFT))
      {
	myFullscreen = !myFullscreen;
	SDL_SetWindowFullscreen(myWindow.get(), myFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
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
    case SDL_SCANCODE_F2:
    {
      quit = true;
      break;
    }
    case SDL_SCANCODE_LALT:
    {
      Paddle::setButtonPressed(Paddle::ourOpenApple);
      break;
    }
    case SDL_SCANCODE_RALT:
    {
      Paddle::setButtonPressed(Paddle::ourSolidApple);
      break;
    }
    }
  }
  std::cerr << "KEY DOWN: " << e.key.keysym.scancode << "," << e.key.keysym.sym << "," << e.key.keysym.mod << "," << bool(e.key.repeat) << std::endl;

}

void Emulator::processKeyUp(const SDL_Event & e)
{
  switch (e.key.keysym.scancode)
  {
  case SDL_SCANCODE_LALT:
  {
    Paddle::setButtonReleased(Paddle::ourOpenApple);
    break;
  }
  case SDL_SCANCODE_RALT:
  {
    Paddle::setButtonReleased(Paddle::ourSolidApple);
    break;
  }
  }
  std::cerr << "KEY UP:   " << e.key.keysym.scancode << "," << e.key.keysym.sym << "," << e.key.keysym.mod << "," << bool(e.key.repeat) << std::endl;
}
