#include "frontends/sdl/sdlcompat.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"

#include <stdexcept>

namespace sa2
{
    namespace compat
    {

#if SDL_VERSION_ATLEAST(3, 0, 0)
        int getNumJoysticks()
        {
            int count = 0;
            const SDL_JoystickID *joy = SDL_GetJoysticks(&count);
            if (!joy)
            {
                throw std::runtime_error(decorateSDLError("SDL_GetJoysticks"));
            }
            return count;
        }

        int getGLSwapInterval()
        {
            int interval = 0;
            if (!SDL_GL_GetSwapInterval(&interval))
            {
                throw std::runtime_error(decorateSDLError("SDL_GL_GetSwapInterval"));
            }
            return interval;
        }

        const SDL_DisplayMode *getCurrentDisplayMode()
        {
            int count = 0;
            const SDL_DisplayID *displays = SDL_GetDisplays(&count);
            if (displays)
            {
                const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(*displays);
                return mode;
            }
            throw std::runtime_error(decorateSDLError("SDL_GetDisplays"));
        }

        void pauseAudioDevice(SDL_AudioDeviceID dev)
        {
            SDL_PauseAudioDevice(dev);
        }

        void resumeAudioDevice(SDL_AudioDeviceID dev)
        {
            SDL_ResumeAudioDevice(dev);
        }

        void toggleCursor()
        {
            if (SDL_CursorVisible() == SDL_TRUE)
            {
                SDL_HideCursor();
            }
            else
            {
                SDL_ShowCursor();
            }
        }

        SDL_Window *createWindow(const char *title, const common2::Geometry &geometry, SDL_WindowFlags flags)
        {
            return SDL_CreateWindow(title, geometry.width, geometry.height, flags);
        }

        SDL_Renderer *createRenderer(SDL_Window *window, const int index)
        {
            // I am not sure whether we should worry about SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER
            const char *name = index >= 0 ? SDL_GetRenderDriver(index) : nullptr;

            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window);
            SDL_SetBooleanProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, SDL_TRUE);
            if (index >= 0)
            {
                SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, SDL_GetRenderDriver(index));
            }

            SDL_Renderer *renderer = SDL_CreateRendererWithProperties(props);
            SDL_DestroyProperties(props);
            return renderer;
        }

        SDL_Surface *createSurfaceFromResource(const unsigned char *data, unsigned int size)
        {
          SDL_IOStream *ops = SDL_IOFromConstMem(data, size);
          if (!ops)
          {
              throw std::runtime_error(decorateSDLError("SDL_IOFromConstMem"));
          }
          SDL_Surface *surface = IMG_Load_IO(ops, 1);
          if (!surface)
          {
              throw std::runtime_error(decorateSDLError("IMG_Load_IO"));
          }
          return surface;
        }

#else

        int getNumJoysticks()
        {
            return SDL_NumJoysticks();
        }

        int getGLSwapInterval()
        {
            return SDL_GL_GetSwapInterval();
        }

        const SDL_DisplayMode *getCurrentDisplayMode()
        {
            static SDL_DisplayMode current;

            const int should_be_zero = SDL_GetCurrentDisplayMode(0, &current);
            if (should_be_zero)
            {
                throw std::runtime_error(sa2::decorateSDLError("SDL_GetCurrentDisplayMode"));
            }
            return &current;
        }

        void pauseAudioDevice(SDL_AudioDeviceID dev)
        {
            SDL_PauseAudioDevice(dev, 1);
        }

        void resumeAudioDevice(SDL_AudioDeviceID dev)
        {
            SDL_PauseAudioDevice(dev, 0);
        }

        void toggleCursor()
        {
            const int current = SDL_ShowCursor(SDL_QUERY);
            SDL_ShowCursor(current ^ 1);
        }

        SDL_Window *createWindow(const char *title, const common2::Geometry &geometry, SDL_WindowFlags flags)
        {
            return SDL_CreateWindow(title, geometry.x, geometry.y, geometry.width, geometry.height, flags);
        }

        SDL_Renderer *createRenderer(SDL_Window *window, const int index)
        {
            return SDL_CreateRenderer(window, index, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        }

        SDL_Surface *createSurfaceFromResource(const unsigned char *data, unsigned int size)
        {
            SDL_RWops *ops = SDL_RWFromConstMem(data, size);
            if (!ops)
            {
                throw std::runtime_error(decorateSDLError("SDL_RWFromConstMem"));
            }
            SDL_Surface *surface = IMG_Load_RW(ops, 1);
            if (!surface)
            {
                throw std::runtime_error(decorateSDLError("IMG_Load_RW"));
            }
            return surface;
        }

#endif

    } // namespace compat
} // namespace sa2
