#include "StdAfx.h"
#include "frontends/sdl/renderer/sdlrendererframe.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"

#include "Interface.h"
#include "Core.h"

#include <iostream>

namespace sa2
{

    SDLRendererFrame::SDLRendererFrame(const common2::EmulatorOptions &options)
        : SDLFrame(options)
    {
        const common2::Geometry geometry = getGeometryOrDefault(options.geometry);

        myWindow.reset(compat::createWindow(g_pAppTitle.c_str(), geometry, SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
        if (!myWindow)
        {
            throw std::runtime_error(decorateSDLError("SDL_CreateWindow"));
        }

        SetApplicationIcon();

        myRenderer.reset(compat::createRenderer(myWindow.get(), options.sdlDriver), SDL_DestroyRenderer);
        if (!myRenderer)
        {
            throw std::runtime_error(decorateSDLError("SDL_CreateRenderer"));
        }

        SetGLSynchronisation(options); // must be called after GL initialisation

        printRendererInfo(std::cerr, myRenderer, ourPixelFormat, options.sdlDriver);
    }

    void SDLRendererFrame::Initialize(bool resetVideoState)
    {
        SDLFrame::Initialize(resetVideoState);
        Video &video = GetVideo();

        const int width = video.GetFrameBufferWidth();
        const int height = video.GetFrameBufferHeight();
        const int sw = video.GetFrameBufferBorderlessWidth();
        const int sh = video.GetFrameBufferBorderlessHeight();

        if (myPreserveAspectRatio && SA2_RENDERER_LOGICAL_SIZE(myRenderer.get(), sw, sh))
        {
            throw std::runtime_error(decorateSDLError("SDL_RenderSetLogicalSize"));
        }

        myTexture.reset(
            SDL_CreateTexture(myRenderer.get(), ourPixelFormat, SDL_TEXTUREACCESS_STATIC, width, height),
            SDL_DestroyTexture);

        myRect.x = video.GetFrameBufferBorderWidth();
        myRect.y = video.GetFrameBufferBorderHeight();
        myRect.w = sw;
        myRect.h = sh;
        myPitch = width * sizeof(bgra_t);
    }

    void SDLRendererFrame::VideoPresentScreen()
    {
        SDL_UpdateTexture(myTexture.get(), nullptr, myFramebuffer.data(), myPitch);
        SDL_RenderClear(myRenderer.get());
        SDL_RenderCopyEx(myRenderer.get(), myTexture.get(), &myRect, nullptr, 0.0, nullptr, SDL_FLIP_VERTICAL);
        SDL_RenderPresent(myRenderer.get());
    }

    void SDLRendererFrame::GetRelativeMousePosition(const SDL_MouseMotionEvent &motion, float &x, float &y) const
    {
        int width, height;
        SDL_GetWindowSize(myWindow.get(), &width, &height);

        x = GetRelativePosition(motion.x, width);
        y = GetRelativePosition(motion.y, height);
    }

    bool SDLRendererFrame::Quit() const
    {
        return false;
    }

    void SDLRendererFrame::ToggleMouseCursor()
    {
        compat::toggleCursor();
    }

} // namespace sa2
