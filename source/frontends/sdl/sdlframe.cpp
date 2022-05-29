#include "StdAfx.h"
#include "frontends/sdl/processfile.h"
#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/utils.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"

#include "CardManager.h"
#include "Core.h"
#include "Utilities.h"
#include "SaveState.h"
#include "Speaker.h"
#include "SoundCore.h"
#include "Interface.h"
#include "NTSC.h"
#include "CPU.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "Log.h"
#include "Debugger/Debug.h"

#include "linux/paddle.h"
#include "linux/keyboard.h"

#include <algorithm>

#include <SDL_image.h>

// #define KEY_LOGGING_VERBOSE

namespace
{

  void processAppleKey(const SDL_KeyboardEvent & key, const bool forceCapsLock)
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
    case SDLK_KP_ENTER:
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
        // same logic as AW
        // CAPS is forced when the emulator starts
        // until is used the first time
        const bool capsLock = forceCapsLock || (SDL_GetModState() & KMOD_CAPS);
        const bool upper = capsLock || (key.keysym.mod & KMOD_SHIFT);

        ch = (key.keysym.sym - SDLK_a) + 0x01;
        if (key.keysym.mod & KMOD_CTRL)
        {
          // ok
        }
        else if (upper)
        {
          ch += 0x40; // upper case
        }
        else
        {
          ch += 0x60; // lower case
        }
        break;
      }
    }

    if (ch)
    {
      addKeyToBuffer(ch);
#ifdef KEY_LOGGING_VERBOSE
      LogOutput("SDL KeyboardEvent: %02x\n", ch);
#endif
    }
  }

}

namespace sa2
{

  SDLFrame::SDLFrame(const common2::EmulatorOptions & options)
    : myTargetGLSwap(options.glSwapInterval)
    , myForceCapsLock(true)
    , myMultiplier(1)
    , myFullscreen(false)
    , myDragAndDropSlot(SLOT6)
    , myDragAndDropDrive(DRIVE_1)
    , myScrollLockFullSpeed(false)
    , mySpeed(options.fixedSpeed)
  {
  }

  void SDLFrame::End()
  {
    CommonFrame::End();
    if (!myFullscreen)
    {
      common2::Geometry geometry;
      SDL_GetWindowPosition(myWindow.get(), &geometry.x, &geometry.y);
      SDL_GetWindowSize(myWindow.get(), &geometry.width, &geometry.height);
      saveGeometryToRegistry("sa2", geometry);
    }
  }

  void SDLFrame::setGLSwapInterval(const int interval)
  {
    const int current = SDL_GL_GetSwapInterval();
    // in QEMU with GL_RENDERER: llvmpipe (LLVM 12.0.0, 256 bits)
    // SDL_GL_SetSwapInterval() always fails
    if (interval != current && SDL_GL_SetSwapInterval(interval))
    {
      throw std::runtime_error(decorateSDLError("SDL_GL_SetSwapInterval"));
    }
  }

  void SDLFrame::Begin()
  {
    CommonFrame::Begin();
    mySpeed.reset();
    setGLSwapInterval(myTargetGLSwap);
    ResetHardware();
  }

  void SDLFrame::FrameRefreshStatus(int drawflags)
  {
    if (drawflags & DRAW_TITLE)
    {
      GetAppleWindowTitle();
      SDL_SetWindowTitle(myWindow.get(), g_pAppTitle.c_str());
    }
  }

  void SDLFrame::SetApplicationIcon()
  {
    const std::string path = getResourcePath("APPLEWIN.ICO");
    std::shared_ptr<SDL_Surface> icon(IMG_Load(path.c_str()), SDL_FreeSurface);
    if (icon)
    {
      SDL_SetWindowIcon(myWindow.get(), icon.get());
    }
  }

  const std::shared_ptr<SDL_Window> & SDLFrame::GetWindow() const
  {
    return myWindow;
  }

  void SDLFrame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
  {
    const std::string filename = getBitmapFilename(lpBitmapName);
    const std::string path = getResourcePath(filename);

    std::shared_ptr<SDL_Surface> surface(SDL_LoadBMP(path.c_str()), SDL_FreeSurface);
    if (surface)
    {
      SDL_LockSurface(surface.get());

      const char * source = static_cast<char *>(surface->pixels);
      const size_t size = surface->h * surface->w / 8;
      const size_t requested = cb;

      const size_t copied = std::min(requested, size);

      char * dest = static_cast<char *>(lpvBits);

      for (size_t i = 0; i < copied; ++i)
      {
        const size_t offset = i * 8;
        char val = 0;
        for (size_t j = 0; j < 8; ++j)
        {
          const char pixel = *(source + offset + j);
          val = (val << 1) | pixel;
        }
        dest[i] = val;
      }

      SDL_UnlockSurface(surface.get());
    }
    else
    {
      CommonFrame::GetBitmap(lpBitmapName, cb, lpvBits);
    }
  }

  int SDLFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
  {
    // tabs do not render properly
    std::string s(lpText);
    std::replace(s.begin(), s.end(), '\t', ' ');
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, lpCaption, s.c_str(), nullptr);
    ResetSpeed();
    return IDOK;
  }

  void SDLFrame::ProcessEvents(bool &quit)
  {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0)
    {
      ProcessSingleEvent(e, quit);
    }
  }

  void SDLFrame::ProcessSingleEvent(const SDL_Event & e, bool &quit)
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
        ProcessKeyDown(e.key);
        break;
      }
    case SDL_KEYUP:
      {
        ProcessKeyUp(e.key);
        break;
      }
    case SDL_TEXTINPUT:
      {
        ProcessText(e.text);
        break;
      }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      {
        ProcessMouseButton(e.button);
        break;
      }
    case SDL_MOUSEMOTION:
      {
        ProcessMouseMotion(e.motion);
        break;
      }
    case SDL_DROPFILE:
      {
        ProcessDropEvent(e.drop);
        SDL_free(e.drop.file);
        break;
      }
    }
  }

  void SDLFrame::ProcessMouseButton(const SDL_MouseButtonEvent & event)
  {
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
      switch (event.button)
      {
      case SDL_BUTTON_LEFT:
      case SDL_BUTTON_RIGHT:
        {
          const eBUTTONSTATE state = (event.state == SDL_PRESSED) ? BUTTON_DOWN : BUTTON_UP;
          const eBUTTON button = (event.button == SDL_BUTTON_LEFT) ? BUTTON0 : BUTTON1;
          cardManager.GetMouseCard()->SetButton(button, state);
          break;
        }
      }
    }
  }

  double SDLFrame::GetRelativePosition(const int value, const int size)
  {
    // the minimum size of a window is 1
    const double result = double(value) / double(std::max(1, size - 1));
    return result;
  }

  void SDLFrame::ProcessMouseMotion(const SDL_MouseMotionEvent & motion)
  {
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
      int iX, iMinX, iMaxX;
      int iY, iMinY, iMaxY;
      cardManager.GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

      int width, height;
      SDL_GetWindowSize(myWindow.get(), &width, &height);
      const bool relative = (iMaxX - iMinX == 32767) && (iMaxY - iMinY == 32767);

      int dx, dy;
      if (relative)
      {
        // in geos the screen in 280 * 2 mouse ticks in x and 192 in y
        // everything works in relative motion
        // so we do not know where the pointer is
        //
        // the risk is that we leave the SDL window before the guest pointer reaches the end
        // and we stop receiving events
        //
        // we could call SDL_CaptureMouse(), but this interacts badly with ImGui
        //
        // so we make the guest pointer a bit faster (alpha > 1)
        //
        // at least the pointer moves at the right speed
        // and with some gymnic, one can put it in the right place

        const double sizeX = 280 * 2;
        const double sizeY = 192;

        const double relX = double(motion.xrel) / double(width) * sizeX;
        const double relY = double(motion.yrel) / double(height) * sizeY;

        const double alpha = 1.1;
        dx = lround(relX * alpha);
        dy = lround(relY * alpha);
      }
      else
      {
        const int sizeX = iMaxX - iMinX;
        const int sizeY = iMaxY - iMinY;


        double x, y;
        GetRelativeMousePosition(motion, x, y);

        const int newX = lround(x * sizeX) + iMinX;
        const int newY = lround(y * sizeY) + iMinY;

        dx = newX - iX;
        dy = newY - iY;
      }

      int outOfBoundsX;
      int outOfBoundsY;
      cardManager.GetMouseCard()->SetPositionRel(dx, dy, &outOfBoundsX, &outOfBoundsY);
    }
  }

  void SDLFrame::ProcessDropEvent(const SDL_DropEvent & drop)
  {
    processFile(this, drop.file, myDragAndDropSlot, myDragAndDropDrive);
  }

  void SDLFrame::ProcessKeyDown(const SDL_KeyboardEvent & key)
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
          LoadSnapshot();
          break;
        }
      case SDLK_F11:
        {
          const std::string & pathname = Snapshot_GetPathname();
          const std::string message = "Do you want to save the state to: " + pathname + "?";
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
          CycleVideoType();
          break;
        }
      case SDLK_F6:
        {
          if ((key.keysym.mod & KMOD_CTRL) && (key.keysym.mod & KMOD_SHIFT))
          {
            Cycle50ScanLines();
          }
          else if (key.keysym.mod & KMOD_CTRL)
          {
            Video & video = GetVideo();
            myMultiplier = myMultiplier == 1 ? 2 : 1;
            const int sw = video.GetFrameBufferBorderlessWidth();
            const int sh = video.GetFrameBufferBorderlessHeight();
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
          sa2::printAudioInfo();
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
          const AppMode_e newMode = (g_nAppMode == MODE_RUNNING) ? MODE_PAUSED : MODE_RUNNING;
          ChangeMode(newMode);
          break;
        }
      case SDLK_CAPSLOCK:
        {
          myForceCapsLock = false;
          break;
        }
      case SDLK_INSERT:
        {
          if (key.keysym.mod & KMOD_SHIFT)
          {
            char * text = SDL_GetClipboardText();
            if (text)
            {
              addTextToBuffer(text);
              SDL_free(text);
            }
          }
          else if (key.keysym.mod & KMOD_CTRL)
          {
            // in AppleWin this is Ctrl-PrintScreen
            // but PrintScreen is not passed to SDL
            char *pText;
            const size_t size = Util_GetTextScreen(pText);
            if (size)
            {
              SDL_SetClipboardText(pText);
            }
          }
          else if (key.keysym.mod & KMOD_ALT)
          {
            Video_TakeScreenShot(Video::SCREENSHOT_560x384);
          }
          break;
        }
      case SDLK_SCROLLLOCK:
        {
          myScrollLockFullSpeed = !myScrollLockFullSpeed;
          break;
        }
      }
    }

    processAppleKey(key, myForceCapsLock);
  }

  void SDLFrame::ProcessKeyUp(const SDL_KeyboardEvent & key)
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
  }

  void SDLFrame::ProcessText(const SDL_TextInputEvent & text)
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
#ifdef KEY_LOGGING_VERBOSE
          LogOutput("SDL TextInputEvent: %02x\n", key);
#endif
          break;
        }
      }
    }
  }

  void SDLFrame::Execute(const DWORD cyclesToExecute)
  {
    const bool bVideoUpdate = !g_bFullSpeed;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

    // do it in the same batches as AppleWin (1 ms)
    const DWORD fExecutionPeriodClks = g_fCurrentCLK6502 * (1.0 / 1000.0);  // 1 ms

    DWORD totalCyclesExecuted = 0;
    // check at the end because we want to always execute at least 1 cycle even for "0"
    do
    {
      const DWORD thisCyclesToExecute = std::min(fExecutionPeriodClks, cyclesToExecute - totalCyclesExecuted);
      const DWORD executedCycles = CpuExecute(thisCyclesToExecute, bVideoUpdate);
      totalCyclesExecuted += executedCycles;

      GetCardMgr().Update(executedCycles);
      SpkrUpdate(executedCycles);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;

    } while (totalCyclesExecuted < cyclesToExecute);
  }

  void SDLFrame::ExecuteInRunningMode(const size_t msNextFrame)
  {
    SetFullSpeed(CanDoFullSpeed());
    const uint64_t cyclesToExecute = mySpeed.getCyclesTillNext(msNextFrame * 1000);  // this checks g_bFullSpeed
    Execute(cyclesToExecute);
  }

  void SDLFrame::ExecuteInDebugMode(const size_t msNextFrame)
  {
    // In AppleWin this is called without a timer for just one iteration
    // because we run a "frame" at a time, we need a bit of ingenuity
    const uint64_t cyclesToExecute = mySpeed.getCyclesAtFixedSpeed(msNextFrame * 1000);
    const uint64_t target = g_nCumulativeCycles + cyclesToExecute;

    while (g_nAppMode == MODE_STEPPING && g_nCumulativeCycles < target)
    {
      DebugContinueStepping();
    }
  }

  void SDLFrame::ExecuteOneFrame(const size_t msNextFrame)
  {
    // when running in adaptive speed
    // the value msNextFrame is only a hint for when the next frame will arrive
    switch (g_nAppMode)
    {
      case MODE_RUNNING:
        {
          ExecuteInRunningMode(msNextFrame);
          break;
        }
      case MODE_STEPPING:
        {
          ExecuteInDebugMode(msNextFrame);
          break;
        }
    };
  }

  void SDLFrame::ResetSpeed()
  {
    mySpeed.reset();
  }

  void SDLFrame::ChangeMode(const AppMode_e mode)
  {
    if (mode != g_nAppMode)
    {
      switch (mode)
      {
      case MODE_RUNNING:
        DebugExitDebugger();
        SoundCore_SetFade(FADE_IN);
        break;
      case MODE_DEBUG:
        DebugBegin();
        CmdWindowViewConsole(0);
        break;
      default:
        g_nAppMode = mode;
        SoundCore_SetFade(FADE_OUT);
        break;
      }
      FrameRefreshStatus(DRAW_TITLE);
      ResetSpeed();
    }
  }

  void SDLFrame::getDragDropSlotAndDrive(size_t & slot, size_t & drive) const
  {
    slot = myDragAndDropSlot;
    drive = myDragAndDropDrive;
  }

  void SDLFrame::setDragDropSlotAndDrive(const size_t slot, const size_t drive)
  {
    myDragAndDropSlot = slot;
    myDragAndDropDrive = drive;
  }

  void SDLFrame::SetFullSpeed(const bool value)
  {
    if (g_bFullSpeed != value)
    {
      if (value)
      {
        // entering full speed
        MB_Mute();
        setGLSwapInterval(0);
        VideoRedrawScreenDuringFullSpeed(0, true);
      }
      else
      {
        // leaving full speed
        MB_Unmute();
        setGLSwapInterval(myTargetGLSwap);
        mySpeed.reset();
      }
      g_bFullSpeed = value;
    }
  }

  bool SDLFrame::CanDoFullSpeed()
  {
    return myScrollLockFullSpeed ||
           (g_dwSpeed == SPEED_MAX) ||
           (GetCardMgr().GetDisk2CardMgr().IsConditionForFullSpeed() && !Spkr_IsActive() && !MB_IsActive()) ||
           IsDebugSteppingAtFullSpeed();
  }

  void SDLFrame::SingleStep()
  {
    SetFullSpeed(CanDoFullSpeed());
    Execute(0);
  }

  void SDLFrame::ResetHardware()
  {
    myHardwareConfig.Reload();
  }

  bool SDLFrame::HardwareChanged() const
  {
    const CConfigNeedingRestart currentConfig = CConfigNeedingRestart::Create();
    return myHardwareConfig != currentConfig;
  }

  void SDLFrame::LoadSnapshot()
  {
    common2::CommonFrame::LoadSnapshot();
    mySpeed.reset();
    ResetHardware();
  }

}

void SingleStep(bool /* bReinit */)
{
  dynamic_cast<sa2::SDLFrame &>(GetFrame()).SingleStep();
}
