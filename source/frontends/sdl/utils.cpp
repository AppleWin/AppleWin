#include "frontends/sdl/utils.h"
#include <ostream>

#define INDENT '\t'

namespace sa2
{

  void printVideoInfo(std::ostream & os)
  {
    os << "SDL Video driver (SDL_VIDEODRIVER): " << SDL_GetCurrentVideoDriver() << std::endl;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
    {
      os << INDENT << SDL_GetVideoDriver(i) << std::endl;
    }
  }

  void printAudioInfo(std::ostream & os)
  {
    os << "SDL Audio driver (SDL_AUDIODRIVER): " << SDL_GetCurrentAudioDriver() << std::endl;
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
    {
      os << INDENT << SDL_GetAudioDriver(i) << std::endl;
    }

    os << "SDL Audio device: ";
    
    const int iscapture = 0;

#if SDL_VERSION_ATLEAST(2, 24, 0)
    char *defaultDevice = nullptr;
    SDL_AudioSpec spec;
    if (!SDL_GetDefaultAudioInfo(&defaultDevice, &spec, iscapture))
    {
      os << defaultDevice;
      SDL_free(defaultDevice);
    }
    else
#endif

    {
      os << "<default>";
    }
    os << std::endl;

    for (int i = 0; i < SDL_GetNumAudioDevices(iscapture); ++i)
    {
      os << INDENT << SDL_GetAudioDeviceName(i, iscapture) << std::endl;
    }
  }

  void printRendererInfo(std::ostream & os,
                         const std::shared_ptr<SDL_Renderer> & ren,
                         const Uint32 pixelFormat,
                         const int selectedDriver)
  {
    SDL_RendererInfo info;
    SDL_GetRendererInfo(ren.get(), &info);

    const size_t n = SDL_GetNumRenderDrivers();
    os << "SDL Render driver:" << std::endl;
    for(size_t i = 0; i < n; ++i)
    {
      SDL_RendererInfo info;
      SDL_GetRenderDriverInfo(i, &info);
      os << INDENT << i << ": " << info.name << std::endl;
    }

    if (SDL_GetRendererInfo(ren.get(), &info) == 0)
    {
      os << "Active driver (" << selectedDriver << "): " << info.name << std::endl;
      os << INDENT << "SDL_RENDERER_SOFTWARE: " << ((info.flags & SDL_RENDERER_SOFTWARE) > 0) << std::endl;
      os << INDENT << "SDL_RENDERER_ACCELERATED: " << ((info.flags & SDL_RENDERER_ACCELERATED) > 0) << std::endl;
      os << INDENT << "SDL_RENDERER_PRESENTVSYNC: " << ((info.flags & SDL_RENDERER_PRESENTVSYNC) > 0) << std::endl;
      os << INDENT << "SDL_RENDERER_TARGETTEXTURE: " << ((info.flags & SDL_RENDERER_TARGETTEXTURE) > 0) << std::endl;

      os << "Pixel format: " << SDL_GetPixelFormatName(pixelFormat) << std::endl;
      for (size_t i = 0; i < info.num_texture_formats; ++i)
      {
        os << INDENT << SDL_GetPixelFormatName(info.texture_formats[i]) << std::endl;
      }
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

  size_t getCanonicalModifiers(const SDL_KeyboardEvent & key)
  {
    // simplification of the handling of left and right version of the modifiers
    // so we can use equality == comparisons
    size_t modifiers = KMOD_NONE;
    if (key.keysym.mod & KMOD_CTRL)
    {
      modifiers |= KMOD_CTRL;
    }
    if (key.keysym.mod & KMOD_SHIFT)
    {
      modifiers |= KMOD_SHIFT;
    }
    if (key.keysym.mod & KMOD_ALT)
    {
      modifiers |= KMOD_ALT;
    }
    return modifiers;
  }

}
