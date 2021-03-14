#include <SDL.h>

#include <iostream>
#include <memory>
#include <iomanip>

#include "StdAfx.h"
#include "linux/interface.h"
#include "linux/benchmark.h"
#include "linux/context.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/timer.h"
#include "frontends/sdl/gamepad.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/utils.h"

#include "frontends/sdl/renderer/sdlrendererframe.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"

#include "CardManager.h"
#include "Core.h"
#include "Log.h"
#include "CPU.h"
#include "NTSC.h"
#include "SaveState.h"
#include "Interface.h"

// comment out to test / debug init / shutdown only
#define EMULATOR_RUN

namespace
{

  int getRefreshRate()
  {
    SDL_DisplayMode current;

    const int should_be_zero = SDL_GetCurrentDisplayMode(0, &current);

    if (should_be_zero)
    {
      throw std::runtime_error(SDL_GetError());
    }

    return current.refresh_rate;
  }

  struct Data
  {
    sa2::SDLFrame * frame;
    SDL_mutex * mutex;
    common2::Timer * timer;
  };

  Uint32 emulator_callback(Uint32 interval, void *param)
  {
    Data * data = static_cast<Data *>(param);
    SDL_LockMutex(data->mutex);

    data->timer->tic();
    data->frame->ExecuteOneFrame(interval);
    data->timer->toc();

    SDL_UnlockMutex(data->mutex);
    return interval;
  }

}

void run_sdl(int argc, const char * argv [])
{
  std::cerr << std::fixed << std::setprecision(2);

  common2::EmulatorOptions options;

  Video & video = GetVideo();
  const int sw = video.GetFrameBufferBorderlessWidth();
  const int sh = video.GetFrameBufferBorderlessHeight();

  options.geometry.width = sw;
  options.geometry.height = sh;
  options.geometry.x = SDL_WINDOWPOS_UNDEFINED;
  options.geometry.y = SDL_WINDOWPOS_UNDEFINED;
  options.memclear = g_nMemoryClearType;
  const bool run = getEmulatorOptions(argc, argv, "SDL2", options);

  if (!run)
    return;

  std::shared_ptr<sa2::SDLFrame> frame;

  if (options.imgui)
  {
    frame.reset(new sa2::SDLImGuiFrame(options));
  }
  else
  {
    frame.reset(new sa2::SDLRendererFrame(options));
  }

  if (SDL_GL_SetSwapInterval(options.glSwapInterval))
  {
    throw std::runtime_error(SDL_GetError());
  }

  SetFrame(frame);

  if (options.log)
  {
    LogInit();
  }

  InitializeFileRegistry(options);

  Paddle::instance.reset(new sa2::Gamepad(0));

  g_nMemoryClearType = options.memclear;

  common2::Initialisation init;
  applyOptions(options);

  const int fps = getRefreshRate();
  std::cerr << "Video refresh rate: " << fps << " Hz, " << 1000.0 / fps << " ms" << std::endl;

#ifdef EMULATOR_RUN
  if (options.benchmark)
  {
    // we need to switch off vsync, otherwise FPS is limited to 60
    // and it will take longer to run
    if (SDL_GL_SetSwapInterval(0))
    {
      throw std::runtime_error(SDL_GetError());
    }

    const auto redraw = [&frame]{
                          frame->UpdateTexture();
                          frame->RenderPresent();
                        };

    const auto refresh = [redraw, &video]{
                           NTSC_SetVideoMode( video.GetVideoMode() );
                           NTSC_VideoRedrawWholeScreen();
                           redraw();
                         };

    VideoBenchmark(redraw, refresh);
  }
  else
  {
    common2::Timer global;
    common2::Timer updateTextureTimer;
    common2::Timer refreshScreenTimer;
    common2::Timer cpuTimer;
    common2::Timer eventTimer;
    common2::Timer frameTimer;

    const std::string globalTag = ". .";
    std::string updateTextureTimerTag, refreshScreenTimerTag, cpuTimerTag, eventTimerTag;

    if (options.multiThreaded)
    {
      refreshScreenTimerTag = "0 .";
      cpuTimerTag           = "1 M";
      eventTimerTag         = "0 M";
      if (options.looseMutex)
      {
        updateTextureTimerTag = "0 .";
      }
      else
      {
        updateTextureTimerTag = "0 M";
      }

      std::shared_ptr<SDL_mutex> mutex(SDL_CreateMutex(), SDL_DestroyMutex);

      Data data;
      data.mutex = mutex.get();
      data.timer = &cpuTimer;
      data.frame = frame.get();

      const SDL_TimerID timer = SDL_AddTimer(options.timerInterval, emulator_callback, &data);

      bool quit = false;
      do
      {
        frameTimer.tic();
        SDL_LockMutex(data.mutex);

        eventTimer.tic();
        sa2::writeAudio();
        frame->ProcessEvents(quit);
        eventTimer.toc();

        if (options.looseMutex)
        {
          // loose mutex
          // unlock early and let CPU run again in the timer callback
          SDL_UnlockMutex(data.mutex);
          // but the texture will be updated concurrently with the CPU updating the video buffer
          // pixels are not atomic, so a pixel error could happen (if pixel changes while being read)
          // on the positive side this will release pressure from CPU and allow for more parallelism
        }

        updateTextureTimer.tic();
        frame->UpdateTexture();
        updateTextureTimer.toc();

        if (!options.looseMutex)
        {
          // safe mutex, only unlock after texture has been updated
          // this will stop the CPU for longer
          SDL_UnlockMutex(data.mutex);
        }

        if (!options.headless)
        {
          refreshScreenTimer.tic();
          frame->RenderPresent();
          refreshScreenTimer.toc();
        }
        frameTimer.toc();
      } while (!quit);

      SDL_RemoveTimer(timer);
      // if the following enough to make sure the timer has finished
      // and wont be called again?
      SDL_LockMutex(data.mutex);
      SDL_UnlockMutex(data.mutex);
    }
    else
    {
      refreshScreenTimerTag = "0 .";
      cpuTimerTag           = "0 .";
      eventTimerTag         = "0 .";
      updateTextureTimerTag = "0 .";

      bool quit = false;

      // it does not need to be exact
      const size_t oneFrame = 1000 / fps;

      do
      {
        frameTimer.tic();

        eventTimer.tic();
        sa2::writeAudio();
        frame->ProcessEvents(quit);
        eventTimer.toc();

        cpuTimer.tic();
        frame->ExecuteOneFrame(oneFrame);
        cpuTimer.toc();

        updateTextureTimer.tic();
        frame->UpdateTexture();
        updateTextureTimer.toc();

        if (!options.headless)
        {
          refreshScreenTimer.tic();
          frame->RenderPresent();
          refreshScreenTimer.toc();
        }
        frameTimer.toc();
      } while (!quit);
    }

    global.toc();

    const char sep[] = "], ";
    std::cerr << "Global:  [" << globalTag << sep << global << std::endl;
    std::cerr << "Frame:   [" << globalTag << sep << frameTimer << std::endl;
    std::cerr << "Screen:  [" << refreshScreenTimerTag << sep << refreshScreenTimer << std::endl;
    std::cerr << "Texture: [" << updateTextureTimerTag << sep << updateTextureTimer << std::endl;
    std::cerr << "Events:  [" << eventTimerTag << sep << eventTimer << std::endl;
    std::cerr << "CPU:     [" << cpuTimerTag << sep << cpuTimer << std::endl;

    const double timeInSeconds = global.getTimeInSeconds();
    const double actualClock = g_nCumulativeCycles / timeInSeconds;
    std::cerr << "Expected clock: " << g_fCurrentCLK6502 << " Hz, " << g_nCumulativeCycles / g_fCurrentCLK6502 << " s" << std::endl;
    std::cerr << "Actual clock:   " << actualClock << " Hz, " << timeInSeconds << " s" << std::endl;
    sa2::stop();
  }
#endif
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
  Paddle::instance.reset();
  SetFrame(std::shared_ptr<FrameBase>());
  SDL_Quit();

  return exit;
}
