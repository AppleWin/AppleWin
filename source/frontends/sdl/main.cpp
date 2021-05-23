#include <SDL.h>

#include <iostream>
#include <memory>
#include <iomanip>

#include "StdAfx.h"
#include "linux/interface.h"
#include "linux/benchmark.h"
#include "linux/context.h"
#include "linux/network/uthernet2.h"

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

  options.geometry.width = sw * 2;
  options.geometry.height = sh * 2;
  options.geometry.x = SDL_WINDOWPOS_UNDEFINED;
  options.geometry.y = SDL_WINDOWPOS_UNDEFINED;
  const bool run = getEmulatorOptions(argc, argv, "SDL2", options);

  if (!run)
    return;

  const Logger logger(options.log);
  g_nMemoryClearType = options.memclear;
  InitializeFileRegistry(options);

  std::shared_ptr<sa2::SDLFrame> frame;
  if (options.imgui)
  {
    frame.reset(new sa2::SDLImGuiFrame(options));
  }
  else
  {
    frame.reset(new sa2::SDLRendererFrame(options));
  }

  Paddle::instance.reset(new sa2::Gamepad(0));
  const Initialisation init(frame);

  if (SDL_GL_SetSwapInterval(options.glSwapInterval))
  {
    throw std::runtime_error(SDL_GetError());
  }

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

    // it does not need to be exact
    const size_t oneFrame = 1000 / fps;

    bool quit = false;

    do
    {
      frameTimer.tic();

      eventTimer.tic();
      sa2::writeAudio();
      processEventsUthernet2(5);
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

    global.toc();

    std::cerr << "Global:  " << global << std::endl;
    std::cerr << "Frame:   " << frameTimer << std::endl;
    std::cerr << "Screen:  " << refreshScreenTimer << std::endl;
    std::cerr << "Texture: " << updateTextureTimer << std::endl;
    std::cerr << "Events:  " << eventTimer << std::endl;
    std::cerr << "CPU:     " << cpuTimer << std::endl;

    const double timeInSeconds = global.getTimeInSeconds();
    const double actualClock = g_nCumulativeCycles / timeInSeconds;
    std::cerr << "Expected clock: " << g_fCurrentCLK6502 << " Hz, " << g_nCumulativeCycles / g_fCurrentCLK6502 << " s" << std::endl;
    std::cerr << "Actual clock:   " << actualClock << " Hz, " << timeInSeconds << " s" << std::endl;
    sa2::stopAudio();
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

  SDL_Quit();

  return exit;
}
