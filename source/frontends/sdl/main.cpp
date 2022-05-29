#include <SDL.h>

#include <iostream>
#include <memory>
#include <iomanip>

#include "StdAfx.h"
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
      throw std::runtime_error(sa2::decorateSDLError("SDL_GetCurrentDisplayMode"));
    }

    return current.refresh_rate;
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

  common2::EmulatorOptions options;

  Video & video = GetVideo();
  const int sw = video.GetFrameBufferBorderlessWidth();
  const int sh = video.GetFrameBufferBorderlessHeight();

  options.geometry.empty = true;
  options.geometry.width = sw * 2;
  options.geometry.height = sh * 2;
  options.geometry.x = SDL_WINDOWPOS_UNDEFINED;
  options.geometry.y = SDL_WINDOWPOS_UNDEFINED;
  const bool run = getEmulatorOptions(argc, argv, "SDL2", options);

  if (!run)
    return;

  const LoggerContext logger(options.log);
  const RegistryContext registryContext(CreateFileRegistry(options));

  common2::loadGeometryFromRegistry("sa2", options.geometry);

  std::shared_ptr<sa2::SDLFrame> frame;
  if (options.imgui)
  {
    frame.reset(new sa2::SDLImGuiFrame(options));
  }
  else
  {
    frame.reset(new sa2::SDLRendererFrame(options));
  }

  std::cerr << "Default GL swap interval: " << SDL_GL_GetSwapInterval() << std::endl;

  std::shared_ptr<Paddle> paddle(new sa2::Gamepad(0));
  const Initialisation init(frame, paddle);
  common2::applyOptions(options);
  frame->Begin();

  common2::setSnapshotFilename(options.snapshotFilename);
  if (options.loadSnapshot)
  {
    frame->LoadSnapshot();
  }

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
    const size_t oneFrame = 1000 / fps;

    bool quit = false;

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

    const double timeInSeconds = global.getTimeInSeconds();
    const double actualClock = g_nCumulativeCycles / timeInSeconds;
    std::cerr << "Expected clock: " << g_fCurrentCLK6502 << " Hz, " << g_nCumulativeCycles / g_fCurrentCLK6502 << " s" << std::endl;
    std::cerr << "Actual clock:   " << actualClock << " Hz, " << timeInSeconds << " s" << std::endl;
    sa2::stopAudio();
  }
  frame->End();
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
