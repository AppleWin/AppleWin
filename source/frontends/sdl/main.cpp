#include <SDL.h>

#include <iostream>
#include <memory>
#include <iomanip>

#include "StdAfx.h"
#include "linux/benchmark.h"
#include "linux/context.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/commoncontext.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/timer.h"
#include "frontends/sdl/gamepad.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/utils.h"

#include "frontends/sdl/renderer/sdlrendererframe.h"
#include "frontends/sdl/imgui/sdlimguiframe.h"

#include "Core.h"
#include "NTSC.h"
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
      throw std::runtime_error(sa2::decorateSDLError("SDL_GetCurrentDisplayMode"));
    }

    return current.refresh_rate ? current.refresh_rate : 60;
  }

  struct Data
  {
    sa2::SDLFrame * frame;
    SDL_mutex * mutex;
    common2::Timer * timer;
  };

}

void run_sdl(int argc, const char * argv [])
{
  std::cerr << std::fixed << std::setprecision(2);

  std::cerr << "SDL Video driver: " << SDL_GetCurrentVideoDriver() << std::endl;
  std::cerr << "SDL Audio driver: " << SDL_GetCurrentAudioDriver() << std::endl;

  common2::EmulatorOptions options;

  const bool run = getEmulatorOptions(argc, argv, "SDL2", options);

  if (!run)
    return;

  const LoggerContext logger(options.log);
  const RegistryContext registryContext(CreateFileRegistry(options));
  const std::shared_ptr<Paddle> paddle = sa2::Gamepad::create(options.gameControllerIndex, options.gameControllerMappingFile);

  std::shared_ptr<sa2::SDLFrame> frame;
  if (options.imgui)
  {
    frame = std::make_shared<sa2::SDLImGuiFrame>(options);
  }
  else
  {
    frame = std::make_shared<sa2::SDLRendererFrame>(options);
  }

  std::cerr << "Default GL swap interval: " << SDL_GL_GetSwapInterval() << std::endl;

  const common2::CommonInitialisation init(frame, paddle, options);

  const int fps = getRefreshRate();
  std::cerr << "Video refresh rate: " << fps << " Hz, " << 1000.0 / fps << " ms" << std::endl;

#ifdef EMULATOR_RUN
  if (options.benchmark)
  {
    // we need to switch off vsync, otherwise FPS is limited to 60
    // and it will take longer to run
    sa2::SDLFrame::setGLSwapInterval(0);

    const auto redraw = [&frame]{
                          frame->VideoPresentScreen();
                        };

    Video & video = GetVideo();
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
    common2::Timer refreshScreenTimer;
    common2::Timer cpuTimer;
    common2::Timer eventTimer;
    common2::Timer frameTimer;

    const std::string globalTag = ". .";
    std::string updateTextureTimerTag, refreshScreenTimerTag, cpuTimerTag, eventTimerTag;

    // it does not need to be exact
    const uint64_t oneFrameMicros = 1000000 / fps;

    bool quit = false;

    do
    {
      frameTimer.tic();

      eventTimer.tic();
      sa2::writeAudio(options.audioBuffer);
      frame->ProcessEvents(quit);
      eventTimer.toc();

      cpuTimer.tic();
      frame->ExecuteOneFrame(oneFrameMicros);
      cpuTimer.toc();

      if (!options.headless)
      {
        refreshScreenTimer.tic();
        if (g_bFullSpeed)
        {
          frame->VideoRedrawScreenDuringFullSpeed(g_dwCyclesThisFrame);
        }
        else
        {
          frame->VideoPresentScreen();
        }
        refreshScreenTimer.toc();
      }

      frameTimer.toc();
    } while (!quit);

    global.toc();

    std::cerr << "Global:  " << global << std::endl;
    std::cerr << "Frame:   " << frameTimer << std::endl;
    std::cerr << "Screen:  " << refreshScreenTimer << std::endl;
    std::cerr << "Events:  " << eventTimer << std::endl;
    std::cerr << "CPU:     " << cpuTimer << std::endl;

    sa2::stopAudio();
  }
#endif
}

int main(int argc, const char * argv [])
{
  //First we need to start up SDL, and make sure it went ok
  const Uint32 flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS;
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
