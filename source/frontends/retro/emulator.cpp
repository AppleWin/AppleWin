#include "frontends/sa2/emulator.h"
#include "frontends/sa2/sdirectsound.h"
#include "frontends/sa2/utils.h"

#include <iostream>

#include "linux/videobuffer.h"
#include "linux/paddle.h"
#include "linux/keyboard.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "CPU.h"
#include "Frame.h"
#include "Video.h"
#include "Windows/WinVideo.h"
#include "NTSC.h"
#include "Mockingboard.h"
#include "Speaker.h"
#include "Utilities.h"
#include "SaveState.h"
#include "SoundCore.h"

// #define KEY_LOGGING_VERBOSE

namespace
{

  void updateWindowTitle(const std::shared_ptr<SDL_Window> & win)
  {
    GetAppleWindowTitle();
    SDL_SetWindowTitle(win.get(), g_pAppTitle.c_str());
  }

  void cycleVideoType(const std::shared_ptr<SDL_Window> & win)
  {
    g_eVideoType++;
    if (g_eVideoType >= NUM_VIDEO_MODES)
      g_eVideoType = 0;

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();

    updateWindowTitle(win);
  }

  void cycle50ScanLines(const std::shared_ptr<SDL_Window> & win)
  {
    VideoStyle_e videoStyle = GetVideoStyle();
    videoStyle = VideoStyle_e(videoStyle ^ VS_HALF_SCANLINES);

    SetVideoStyle(videoStyle);

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();

    updateWindowTitle(win);
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
      std::cerr << "SDL KeyboardEvent: " << std::hex << (int)ch << std::dec << std::endl;
    }
  }

}


Emulator::Emulator(
  const std::shared_ptr<SDL_Window> & window,
  const std::shared_ptr<SDL_Renderer> & renderer,
  const std::shared_ptr<SDL_Texture> & texture,
  const bool fixedSpeed
)
  : myWindow(window)
  , myRenderer(renderer)
  , myTexture(texture)
  , myMultiplier(1)
  , myFullscreen(false)
  , mySpeed(fixedSpeed)
{
  myRect.x = GetFrameBufferBorderWidth();
  myRect.y = GetFrameBufferBorderHeight();
  myRect.w = GetFrameBufferBorderlessWidth();
  myRect.h = GetFrameBufferBorderlessHeight();
  myPitch = GetFrameBufferWidth() * sizeof(bgra_t);
}

void Emulator::execute(const size_t next)
{
  if (g_nAppMode == MODE_RUNNING)
  {
    const size_t cyclesToExecute = mySpeed.getCyclesTillNext(next);

    const bool bVideoUpdate = true;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

    const DWORD executedCycles = CpuExecute(cyclesToExecute, bVideoUpdate);

    g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;
    GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedCycles);
    MB_PeriodicUpdate(executedCycles);
    SpkrUpdate(executedCycles);
  }
}

void Emulator::updateTexture()
{
  SDL_UpdateTexture(myTexture.get(), nullptr, g_pFramebufferbits, myPitch);
}

void Emulator::refreshVideo()
{
  SDL_RenderCopyEx(myRenderer.get(), myTexture.get(), &myRect, nullptr, 0.0, nullptr, SDL_FLIP_VERTICAL);
  SDL_RenderPresent(myRenderer.get());
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
    case SDLK_F12:
    {
      Snapshot_LoadState();
      mySpeed.reset();
      break;
    }
    case SDLK_F11:
    {
      const std::string & pathname = Snapshot_GetPathname();
      const std::string message = "Do you want to save the state to " + pathname + "?";
      SoundCore_SetFade(FADE_OUT);
      if (show_yes_no_dialog(myWindow, "Save state", message))
      {
	Snapshot_SaveState();
      }
      SoundCore_SetFade(FADE_IN);
      mySpeed.reset();
      break;
    }
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
      if (key.keysym.mod & KMOD_CTRL)
      {
	CtrlReset();
      }
      else
      {
	ResetMachineState();
      }
      break;
    }
    case SDLK_F1:
    {
      SDirectSound::printInfo();
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
    case SDLK_PAUSE:
    {
      switch (g_nAppMode)
      {
      case MODE_RUNNING:
	g_nAppMode = MODE_PAUSED;
	SoundCore_SetFade(FADE_OUT);
	break;
      case MODE_PAUSED:
	g_nAppMode = MODE_RUNNING;
	SoundCore_SetFade(FADE_IN);
	mySpeed.reset();
	break;
      }
      updateWindowTitle(myWindow);
      break;
    }
    }
  }

  processAppleKey(key);

#ifdef KEY_LOGGING_VERBOSE
  std::cerr << "KEY DOWN: " << key.keysym.scancode << "," << key.keysym.sym << "," << key.keysym.mod << "," << bool(key.repeat) << std::endl;
#endif

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

#ifdef KEY_LOGGING_VERBOSE
  std::cerr << "KEY UP:   " << key.keysym.scancode << "," << key.keysym.sym << "," << key.keysym.mod << "," << bool(key.repeat) << std::endl;
#endif

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
      std::cerr << "SDL TextInputEvent: " << std::hex << (int)key << std::dec << std::endl;
      break;
    }
    }
  }
}
