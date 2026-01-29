#include "frontends/sdl/sdlcompat.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"

#include <stdexcept>
#include <cstring>

namespace sa2
{
    namespace compat
    {

#if SDL_VERSION_ATLEAST(3, 0, 0)

        std::vector<SDL_JoystickID> getGameControllers()
        {
            int count = 0;
            SDL_JoystickID *ids = SDL_GetJoysticks(&count);
            if (!ids)
            {
                throw std::runtime_error(decorateSDLError("SDL_GetJoysticks"));
            }
            std::vector<SDL_JoystickID> result;
            for (int i = 0; i < count; ++i)
            {
                if (SDL_IsGamepad(ids[i]))
                {
                    result.push_back(ids[i]);
                }
            }
            SDL_free(ids);
            return result;
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

        const SDL_DisplayMode *getCurrentDisplayMode(SDL_DisplayMode &)
        {
            int count = 0;
            SDL_DisplayID *displays = SDL_GetDisplays(&count);
            if (!displays)
            {
                throw std::runtime_error(decorateSDLError("SDL_GetDisplays"));
            }
            const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(*displays);
            SDL_free(displays);
            return mode;
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
            if (SDL_CursorVisible())
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
            SDL_SetBooleanProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, true);
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

        bool convertAudio(
            const SDL_AudioSpec &wavSpec, const Uint8 *wavBuffer, const Uint32 wavLength, std::vector<int8_t> &output)
        {
            const SDL_AudioFormat format = sizeof(int8_t) == 1 ? AUDIO_S8 : AUDIO_S16SYS;

            const SDL_AudioSpec dstSpec = {format, wavSpec.channels, wavSpec.freq};
            Uint8 *dstData = nullptr;
            int dstLen = 0;
            const bool ok = SDL_ConvertAudioSamples(&wavSpec, wavBuffer, wavLength, &dstSpec, &dstData, &dstLen);
            std::shared_ptr<Uint8> dstBuffer(dstData, SDL_free);

            if (ok)
            {
                output.resize(dstLen / sizeof(int8_t));
                std::memcpy(output.data(), dstBuffer.get(), dstLen);
            }

            return ok;
        }

#else

        std::vector<int> getGameControllers()
        {
            const int count = SDL_NumJoysticks();
            if (count < 0)
            {
                throw std::runtime_error(decorateSDLError("SDL_NumJoysticks"));
            }
            std::vector<int> result;
            for (int i = 0; i < count; ++i)
            {
                if (SDL_IsGameController(i))
                {
                    result.push_back(i);
                }
            }
            return result;
        }

        int getGLSwapInterval()
        {
            return SDL_GL_GetSwapInterval();
        }

        const SDL_DisplayMode *getCurrentDisplayMode(SDL_DisplayMode &dummy)
        {
            if (SDL_GetCurrentDisplayMode(0, &dummy))
            {
                throw std::runtime_error(sa2::decorateSDLError("SDL_GetCurrentDisplayMode"));
            }
            return &dummy;
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

        bool convertAudio(
            const SDL_AudioSpec &wavSpec, const Uint8 *wavBuffer, const Uint32 wavLength, std::vector<int8_t> &output)
        {
            const SDL_AudioFormat format = sizeof(int8_t) == 1 ? AUDIO_S8 : AUDIO_S16SYS;

            SDL_AudioCVT cvt;
            const int res =
                SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, wavSpec.freq, format, 1, wavSpec.freq);
            if (res < 0)
            {
                return false;
            }

            cvt.len = wavLength;
            output.resize(cvt.len_mult * cvt.len / sizeof(int8_t));
            std::memcpy(output.data(), wavBuffer, cvt.len);

            if (cvt.needed)
            {
                cvt.buf = reinterpret_cast<Uint8 *>(output.data());
                SDL_ConvertAudio(&cvt);
                output.resize(cvt.len_cvt / sizeof(int8_t));
            }

            return true;
        }

#endif

    } // namespace compat
} // namespace sa2
