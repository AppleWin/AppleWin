#include "StdAfx.h"
#include "frontends/sdl/sdlframe.h"

#include "Core.h"
#include "Utilities.h"

#include <iostream>
#include <algorithm>

#include <SDL_image.h>

SDLFrame::SDLFrame()
{
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
  const std::string path = myResourcePath + "APPLEWIN.ICO";
  std::shared_ptr<SDL_Surface> icon(IMG_Load(path.c_str()), SDL_FreeSurface);
  if (icon)
  {
    SDL_SetWindowIcon(myWindow.get(), icon.get());
  }
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

void SDLFrame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
{
  const std::string filename = getBitmapFilename(lpBitmapName);
  const std::string path = myResourcePath + filename;

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
  return IDOK;
}
