#include "StdAfx.h"
#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/resources.h"

#include "Interface.h"
#include "Core.h"
#include "Utilities.h"

#include <iostream>
#include <iomanip>

#include <SDL_image.h>

SDLFrame::SDLFrame(const EmulatorOptions & options)
{
  Video & video = GetVideo();

  const int width = video.GetFrameBufferWidth();
  const int height = video.GetFrameBufferHeight();
  const int sw = video.GetFrameBufferBorderlessWidth();
  const int sh = video.GetFrameBufferBorderlessHeight();

  std::cerr << std::fixed << std::setprecision(2);

  myWindow.reset(SDL_CreateWindow(g_pAppTitle.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, sw, sh, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
  if (!myWindow)
  {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    return;
  }

  SetApplicationIcon();

  myRenderer.reset(SDL_CreateRenderer(myWindow.get(), options.sdlDriver, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC), SDL_DestroyRenderer);
  if (!myRenderer)
  {
    std::cout << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
    return;
  }

  const Uint32 format = SDL_PIXELFORMAT_ARGB8888;
  printRendererInfo(std::cerr, myRenderer, format, options.sdlDriver);

  myTexture.reset(SDL_CreateTexture(myRenderer.get(), format, SDL_TEXTUREACCESS_STATIC, width, height), SDL_DestroyTexture);

  myRect.x = video.GetFrameBufferBorderWidth();
  myRect.y = video.GetFrameBufferBorderHeight();
  myRect.w = video.GetFrameBufferBorderlessWidth();
  myRect.h = video.GetFrameBufferBorderlessHeight();
  myPitch = video.GetFrameBufferWidth() * sizeof(bgra_t);
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
  const std::string path = getResourcePath() + "APPLEWIN.ICO";
  std::shared_ptr<SDL_Surface> icon(IMG_Load(path.c_str()), SDL_FreeSurface);
  if (icon)
  {
    SDL_SetWindowIcon(myWindow.get(), icon.get());
  }
}

void SDLFrame::UpdateTexture()
{
  SDL_UpdateTexture(myTexture.get(), nullptr, myFramebuffer.data(), myPitch);
}

void SDLFrame::RenderPresent()
{
  SDL_RenderCopyEx(myRenderer.get(), myTexture.get(), &myRect, nullptr, 0.0, nullptr, SDL_FLIP_VERTICAL);
  SDL_RenderPresent(myRenderer.get());
}

void SDLFrame::VideoPresentScreen()
{
  UpdateTexture();
  RenderPresent();
}

const std::shared_ptr<SDL_Window> & SDLFrame::GetWindow() const
{
  return myWindow;
}
