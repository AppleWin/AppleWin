#include "frontends/sa2/utils.h"
#include <ostream>

void printRendererInfo(std::ostream & os, std::shared_ptr<SDL_Renderer> & ren, const Uint32 pixelFormat)
{
  SDL_RendererInfo info;
  SDL_GetRendererInfo(ren.get(), &info);

  const size_t n = SDL_GetNumRenderDrivers();
  os << "SDL: " << n << " drivers" << std::endl;
  for(size_t i = 0; i < n; ++i)
  {
      SDL_RendererInfo info;
      SDL_GetRenderDriverInfo(i, &info);
      os << " " << i << ": " << info.name << std::endl;
  }

  if (SDL_GetRendererInfo(ren.get(), &info) == 0)
  {
    os << "Active driver: " << info.name << std::endl;
    os << " SDL_RENDERER_SOFTWARE: " << ((info.flags & SDL_RENDERER_SOFTWARE) > 0) << std::endl;
    os << " SDL_RENDERER_ACCELERATED: " << ((info.flags & SDL_RENDERER_ACCELERATED) > 0) << std::endl;
    os << " SDL_RENDERER_PRESENTVSYNC: " << ((info.flags & SDL_RENDERER_PRESENTVSYNC) > 0) << std::endl;
    os << " SDL_RENDERER_TARGETTEXTURE: " << ((info.flags & SDL_RENDERER_TARGETTEXTURE) > 0) << std::endl;

    os << "Supported pixel formats:" << std::endl;
    for (size_t i = 0; i < info.num_texture_formats; ++i)
    {
      os << " " << SDL_GetPixelFormatName(info.texture_formats[i]) << std::endl;
    }
    os << "Selected format: " << SDL_GetPixelFormatName(pixelFormat) << std::endl;
  }
  else
  {
    os << "No Renderinfo" << std::endl;
  }
}
