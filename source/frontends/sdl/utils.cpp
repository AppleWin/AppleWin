#include "frontends/sdl/utils.h"
#include <ostream>

namespace sa2
{

  void printRendererInfo(std::ostream & os,
                         const std::shared_ptr<SDL_Renderer> & ren,
                         const Uint32 pixelFormat,
                         const int selectedDriver)
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
      os << "Active driver (" << selectedDriver << "): " << info.name << std::endl;
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

  bool show_yes_no_dialog(const std::shared_ptr<SDL_Window> & win,
                          const std::string & title,
                          const std::string & text)
  {
    const SDL_MessageBoxButtonData buttons[] =
      {
       { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "yes" },
       { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "no" },
      };

    const SDL_MessageBoxData messageboxdata =
      {
       SDL_MESSAGEBOX_INFORMATION,
       win.get(),
       title.c_str(),
       text.c_str(),
       SDL_arraysize(buttons),
       buttons,
       nullptr
      };

    int buttonid;
    if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0) {
      return false;
    }

    return buttonid == 0;
  }

  std::string decorateSDLError(const std::string & prefix)
  {
    return prefix + ": " + SDL_GetError();
  }

}
