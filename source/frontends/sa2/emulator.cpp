#include "frontends/sa2/emulator.h"

#include <iostream>

#include "linux/data.h"
#include "linux/paddle.h"
#include "linux/keyboard.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "AppleWin.h"
#include "Disk.h"
#include "CPU.h"
#include "Frame.h"
#include "Video.h"
#include "NTSC.h"
#include "Mockingboard.h"
#include "Speaker.h"

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
    SDL_UpdateTexture(tex.get(), nullptr, data, width * 4);

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

  void processAppleKey(const SDL_KeyboardEvent & key)
  {
    // using keycode (or scan code) one takes a physical view of the keyboard
    // ignoring non US layouts
    // SHIFT-3 on a A2 keyboard in # while on my UK keyboard is £?
    // so for now all ASCII keys are handled as text below
    // but this makes it impossible to detect CTRL-ASCII... more to follow
    BYTE ch = 0;

    switch (key.keysym.sym)
    {
    case SDLK_RETURN:
      {
	ch = 0x0d;
	break;
      }
    case SDLK_BACKSPACE: // same as AppleWin
    case SDLK_LEFT:
      {
	ch = 0x08;
	break;
      }
    case SDLK_RIGHT:
      {
	ch = 0x15;
	break;
      }
    case SDLK_UP:
      {
	ch = 0x0b;
	break;
      }
    case SDLK_DOWN:
      {
	ch = 0x0a;
	break;
      }
    case SDLK_DELETE:
      {
	ch = 0x7f;
	break;
      }
    case SDLK_ESCAPE:
      {
	ch = 0x1b;
	break;
      }
    case SDLK_TAB:
      {
	ch = 0x09;
	break;
      }
    case SDLK_a ... SDLK_z:
      {
	ch = (key.keysym.sym - SDLK_a) + 0x01;
	if (key.keysym.mod & KMOD_CTRL)
	{
	  // ok
	}
	else if (key.keysym.mod & KMOD_SHIFT)
	{
	  ch += 0x60;
	}
	else
	{
	  ch += 0x40;
	}
	break;
      }
    }

    if (ch)
    {
      addKeyToBuffer(ch);
      std::cerr << "Apple Key Down: " << std::hex << (int)ch << std::dec << std::endl;
    }
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
  , myExtraCycles(0)
{
}

void Emulator::executeOneFrame()
{
  const bool bVideoUpdate = true;
  const int nExecutionPeriodUsec = 1000;		// 1.0ms
  const double fUsecPerSec = 1.e6;
  const double fExecutionPeriodClks = int(g_fCurrentCLK6502 * (nExecutionPeriodUsec / fUsecPerSec));

  int totalCyclesExecuted = 0;
  const DWORD uCyclesToExecute = int(g_fCurrentCLK6502 / myFPS) + myExtraCycles;
  const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

  while (totalCyclesExecuted < uCyclesToExecute)
  {
    // go in small steps of 1ms
    const DWORD uActualCyclesExecuted = CpuExecute(fExecutionPeriodClks, bVideoUpdate);
    totalCyclesExecuted += uActualCyclesExecuted;
    g_dwCyclesThisFrame = (g_dwCyclesThisFrame + uActualCyclesExecuted) % dwClksPerFrame;

    GetCardMgr().GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);
    MB_PeriodicUpdate(uActualCyclesExecuted);
    SpkrUpdate(uActualCyclesExecuted);
  }

  myExtraCycles = uCyclesToExecute - totalCyclesExecuted;
}

SDL_Rect Emulator::updateTexture()
{
  return refreshTexture(myTexture);
}

void Emulator::refreshVideo(const SDL_Rect & rect)
{
  renderScreen(myRenderer, myTexture, rect);
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
      processKeyDown(e.key, quit);
      break;
    }
    case SDL_KEYUP:
    {
      processKeyUp(e.key);
      break;
    }
    case SDL_TEXTINPUT:
    {
      processText(e.text);
      break;
    }
    }
  }
}

void Emulator::processKeyDown(const SDL_KeyboardEvent & key, bool & quit)
{
  // scancode vs keycode
  // scancode is relative to the position on the keyboard
  // keycode is what the os maps it to
  // if the user has decided to change the layout, we just go with it and use the keycode
  if (!key.repeat)
  {
    switch (key.keysym.sym)
    {
    case SDLK_F9:
    {
      cycleVideoType(myWindow);
      break;
    }
    case SDLK_F6:
    {
      if ((key.keysym.mod & KMOD_CTRL) && (key.keysym.mod & KMOD_SHIFT))
      {
	cycle50ScanLines(myWindow);
      }
      else if (key.keysym.mod & KMOD_CTRL)
      {
	myMultiplier = myMultiplier == 1 ? 2 : 1;
	const int sw = GetFrameBufferBorderlessWidth();
	const int sh = GetFrameBufferBorderlessHeight();
	SDL_SetWindowSize(myWindow.get(), sw * myMultiplier, sh * myMultiplier);
      }
      else if (!(key.keysym.mod & KMOD_SHIFT))
      {
	myFullscreen = !myFullscreen;
	SDL_SetWindowFullscreen(myWindow.get(), myFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
      }
      break;
    }
    case SDLK_F5:
    {
      CardManager & cardManager = GetCardMgr();
      if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
      {
	dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6))->DriveSwap();
      }
      break;
    }
    case SDLK_F2:
    {
      quit = true;
      break;
    }
    case SDLK_LALT:
    {
      Paddle::setButtonPressed(Paddle::ourOpenApple);
      break;
    }
    case SDLK_RALT:
    {
      Paddle::setButtonPressed(Paddle::ourSolidApple);
      break;
    }
    }
  }

  processAppleKey(key);

  std::cerr << "KEY DOWN: " << key.keysym.scancode << "," << key.keysym.sym << "," << key.keysym.mod << "," << bool(key.repeat) << std::endl;

}

void Emulator::processKeyUp(const SDL_KeyboardEvent & key)
{
  switch (key.keysym.sym)
  {
  case SDLK_LALT:
  {
    Paddle::setButtonReleased(Paddle::ourOpenApple);
    break;
  }
  case SDLK_RALT:
  {
    Paddle::setButtonReleased(Paddle::ourSolidApple);
    break;
  }
  }
  std::cerr << "KEY UP:   " << key.keysym.scancode << "," << key.keysym.sym << "," << key.keysym.mod << "," << bool(key.repeat) << std::endl;
}

void Emulator::processText(const SDL_TextInputEvent & text)
{
  if (strlen(text.text) == 1)
  {
    const char key = text.text[0];
    switch (key) {
    case 0x20 ... 0x40:
    case 0x5b ... 0x60:
    case 0x7b ... 0x7e:
    {
      // not the letters
      // this is very simple, but one cannot handle CRTL-key combination.
      addKeyToBuffer(key);
      std::cerr << "Apple Text: " << std::hex << (int)key << std::dec << std::endl;
      break;
    }
    }
  }
}
