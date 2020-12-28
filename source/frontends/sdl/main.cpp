#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <memory>
#include <iomanip>

#include "linux/interface.h"
#include "linux/benchmark.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/timer.h"
#include "frontends/common2/resources.h"
#include "frontends/sdl/emulator.h"
#include "frontends/sdl/gamepad.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/utils.h"

#include "StdAfx.h"
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
    Emulator * emulator;
    SDL_mutex * mutex;
    Timer * timer;
  };

  Uint32 emulator_callback(Uint32 interval, void *param)
  {
    Data * data = static_cast<Data *>(param);
    SDL_LockMutex(data->mutex);

    data->timer->tic();
    data->emulator->execute(interval);
    data->timer->toc();

    SDL_UnlockMutex(data->mutex);
    return interval;
  }

  void setApplicationIcon(const std::shared_ptr<SDL_Window> & win)
  {
    const std::string path = getResourcePath() + "APPLEWIN.ICO";
    std::shared_ptr<SDL_Surface> icon(IMG_Load(path.c_str()), SDL_FreeSurface);
    if (icon)
    {
      SDL_SetWindowIcon(win.get(), icon.get());
    }
  }

}

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
  // tabs do not render properly
  std::string s(text);
  std::replace(s.begin(), s.end(), '\t', ' ');
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, caption, s.c_str(), nullptr);
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

  InitializeFileRegistry(options);

  Paddle::instance.reset(new Gamepad(0));

  g_nMemoryClearType = options.memclear;

  initialiseEmulator();
  applyOptions(options);

  Video & video = GetVideo();

  const int width = video.GetFrameBufferWidth();
  const int height = video.GetFrameBufferHeight();
  const int sw = video.GetFrameBufferBorderlessWidth();
  const int sh = video.GetFrameBufferBorderlessHeight();

  std::cerr << std::fixed << std::setprecision(2);

  std::shared_ptr<SDL_Window> win(SDL_CreateWindow(g_pAppTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, sw, sh, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
  if (!win)
  {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    return;
  }

  setApplicationIcon(win);

  std::shared_ptr<SDL_Renderer> ren(SDL_CreateRenderer(win.get(), options.sdlDriver, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
  if (!ren)
  {
    std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
    return;
  }

  const Uint32 format = SDL_PIXELFORMAT_ARGB8888;
  printRendererInfo(std::cerr, ren, format, options.sdlDriver);

  std::shared_ptr<SDL_Texture> tex(SDL_CreateTexture(ren.get(), format, SDL_TEXTUREACCESS_STATIC, width, height), SDL_DestroyTexture);

  const int fps = getRefreshRate();
  std::cerr << "Video refresh rate: " << fps << " Hz, " << 1000.0 / fps << " ms" << std::endl;

  Emulator emulator(win, ren, tex, options.fixedSpeed);

#ifdef EMULATOR_RUN
  if (options.benchmark)
  {
    // we need to switch off vsync, otherwise FPS is limited to 60
    // and it will take longer to run
    const int res = SDL_GL_SetSwapInterval(0);
    // if this fails, should we throw, print something or just ignore?

    const auto redraw = [&emulator, res]{
			  emulator.updateTexture();
			  if (res == 0) {
			    emulator.refreshVideo();
			  }
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
    Timer global;
    Timer updateTextureTimer;
    Timer refreshScreenTimer;
    Timer cpuTimer;
    Timer eventTimer;

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
      data.emulator = &emulator;
      data.timer = &cpuTimer;

      const SDL_TimerID timer = SDL_AddTimer(options.timerInterval, emulator_callback, &data);

      bool quit = false;
      do
      {
	SDL_LockMutex(data.mutex);

	eventTimer.tic();
	SDirectSound::writeAudio();
	emulator.processEvents(quit);
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
	emulator.updateTexture();
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
	  emulator.refreshVideo();
	  refreshScreenTimer.toc();
	}

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
	eventTimer.tic();
	SDirectSound::writeAudio();
	emulator.processEvents(quit);
	eventTimer.toc();

	cpuTimer.tic();
	emulator.execute(oneFrame);
	cpuTimer.toc();

	updateTextureTimer.tic();
	emulator.updateTexture();
	updateTextureTimer.toc();

	if (!options.headless)
	{
	  refreshScreenTimer.tic();
	  emulator.refreshVideo();
	  refreshScreenTimer.toc();
	}
      } while (!quit);
    }

    global.toc();

    const char sep[] = "], ";
    std::cerr << "Global:  [" << globalTag << sep << global << std::endl;
    std::cerr << "Events:  [" << eventTimerTag << sep << eventTimer << std::endl;
    std::cerr << "Texture: [" << updateTextureTimerTag << sep << updateTextureTimer << std::endl;
    std::cerr << "Screen:  [" << refreshScreenTimerTag << sep << refreshScreenTimer << std::endl;
    std::cerr << "CPU:     [" << cpuTimerTag << sep << cpuTimer << std::endl;

    const double timeInSeconds = global.getTimeInSeconds();
    const double actualClock = g_nCumulativeCycles / timeInSeconds;
    std::cerr << "Expected clock: " << g_fCurrentCLK6502 << " Hz, " << g_nCumulativeCycles / g_fCurrentCLK6502 << " s" << std::endl;
    std::cerr << "Actual clock:   " << actualClock << " Hz, " << timeInSeconds << " s" << std::endl;
    SDirectSound::stop();
  }
#endif

  uninitialiseEmulator();
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
  SDL_Quit();

  return exit;
}
