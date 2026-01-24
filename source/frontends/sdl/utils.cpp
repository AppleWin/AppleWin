#include "frontends/sdl/utils.h"
#include <ostream>

#define INDENT '\t'

namespace
{

#if SDL_VERSION_ATLEAST(3, 0, 0)
    void processProperty(void *userdata, SDL_PropertiesID props, const char *name)
    {
        std::ostream &os = *reinterpret_cast<std::ostream *>(userdata);
        const char *value = SDL_GetStringProperty(props, name, nullptr);
        if (value)
        {

            os << INDENT << name << ": " << value << std::endl;
        }
    }
#endif

} // namespace

namespace sa2
{

    void printVideoInfo(std::ostream &os)
    {
        os << "SDL Video driver (SDL_VIDEODRIVER): " << SDL_GetCurrentVideoDriver() << std::endl;
        for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i)
        {
            os << INDENT << SDL_GetVideoDriver(i) << std::endl;
        }
    }

    void printAudioInfo(std::ostream &os)
    {
        os << "SDL Audio driver (SDL_AUDIODRIVER): " << SDL_GetCurrentAudioDriver() << std::endl;
        for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i)
        {
            os << INDENT << SDL_GetAudioDriver(i) << std::endl;
        }

        os << "SDL Audio device: ";

        const int iscapture = 0;

#if SDL_VERSION_ATLEAST(3, 0, 0)
        // not relevant in SDL3
#elif SDL_VERSION_ATLEAST(2, 24, 0)
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

#if !SDL_VERSION_ATLEAST(3, 0, 0)
        for (int i = 0; i < SDL_GetNumAudioDevices(iscapture); ++i)
        {
            os << INDENT << SDL_GetAudioDeviceName(i, iscapture) << std::endl;
        }
#endif
    }

    void printRendererInfo(
        std::ostream &os, const std::shared_ptr<SDL_Renderer> &ren, const PixelFormat_t pixelFormat,
        const int selectedDriver)
    {
        const size_t n = SDL_GetNumRenderDrivers();
        os << "SDL Render drivers:" << std::endl;
        for (size_t i = 0; i < n; ++i)
        {
#if SDL_VERSION_ATLEAST(3, 0, 0)
            os << INDENT << i << ": " << SDL_GetRenderDriver(i) << std::endl;
#else
            SDL_RendererInfo info;
            SDL_GetRenderDriverInfo(i, &info);
            os << INDENT << i << ": " << info.name << std::endl;
#endif
        }

        os << "Active driver (" << selectedDriver << "): ";
#if SDL_VERSION_ATLEAST(3, 0, 0)
        os << SDL_GetRendererName(ren.get()) << std::endl;
        SDL_EnumerateProperties(SDL_GetRendererProperties(ren.get()), processProperty, &os);
#else
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(ren.get(), &info) == 0)
        {
            os << info.name << std::endl;
            os << INDENT << "SDL_RENDERER_PRESENTVSYNC: " << ((info.flags & SDL_RENDERER_PRESENTVSYNC) > 0)
               << std::endl;
            os << INDENT << "SDL_RENDERER_SOFTWARE: " << ((info.flags & SDL_RENDERER_SOFTWARE) > 0) << std::endl;
            os << INDENT << "SDL_RENDERER_ACCELERATED: " << ((info.flags & SDL_RENDERER_ACCELERATED) > 0) << std::endl;
            os << INDENT << "SDL_RENDERER_TARGETTEXTURE: " << ((info.flags & SDL_RENDERER_TARGETTEXTURE) > 0)
               << std::endl;
            os << "Pixel format: " << SDL_GetPixelFormatName(pixelFormat) << std::endl;
            for (size_t i = 0; i < info.num_texture_formats; ++i)
            {
                os << INDENT << SDL_GetPixelFormatName(info.texture_formats[i]) << std::endl;
            }
        }
        else
        {
            os << "#NA" << std::endl;
        }
#endif
    }

    bool show_yes_no_dialog(const std::shared_ptr<SDL_Window> &win, const std::string &title, const std::string &text)
    {
        const SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "yes"},
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "no"},
        };

        const SDL_MessageBoxData messageboxdata = {SDL_MESSAGEBOX_INFORMATION, win.get(), title.c_str(), text.c_str(),
                                                   SDL_arraysize(buttons),     buttons,   nullptr};

        int buttonid;
        if (!SA2_OK(SDL_ShowMessageBox(&messageboxdata, &buttonid)))
        {
            return false;
        }

        return buttonid == 0;
    }

    std::string decorateSDLError(const std::string &prefix)
    {
        return prefix + ": " + SDL_GetError();
    }

    size_t getCanonicalModifiers(const SDL_KeyboardEvent &key)
    {
        // simplification of the handling of left and right version of the modifiers
        // so we can use equality == comparisons
        size_t modifiers = KMOD_NONE;
        if (SA2_KEY_MOD(key) & KMOD_CTRL)
        {
            modifiers |= KMOD_CTRL;
        }
        if (SA2_KEY_MOD(key) & KMOD_SHIFT)
        {
            modifiers |= KMOD_SHIFT;
        }
        if (SA2_KEY_MOD(key) & KMOD_ALT)
        {
            modifiers |= KMOD_ALT;
        }
        return modifiers;
    }

} // namespace sa2
