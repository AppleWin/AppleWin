#pragma once

#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/glselector.h"

namespace sa2
{

    class SDLImGuiFrame : public SDLFrame
    {
    public:
        SDLImGuiFrame(const common2::EmulatorOptions &options);

        ~SDLImGuiFrame() override;

        void VideoPresentScreen() override;
        void ResetSpeed() override;
        void Initialize(bool resetVideoState) override;

        bool Quit() const override;

    protected:
        void ProcessSingleEvent(const SDL_Event &event, bool &quit) override;
        void ProcessKeyDown(const SDL_KeyboardEvent &key, bool &quit) override;
        void GetRelativeMousePosition(const SDL_MouseMotionEvent &motion, float &x, float &y) const override;
        void ToggleMouseCursor() override;

    private:
        void UpdateTexture();
        void ClearBackground();
        void DrawAppleVideo();

        size_t myPitch;
        size_t myOffset;
        size_t myBorderlessWidth;
        size_t myBorderlessHeight;
        float myOriginalAspectRatio;

        float myDeadTopZone; // for mouse position
        bool myPresenting;   // VideoPresentScreen() is NOT REENTRANT
        bool myShowMouseCursor;

        SDL_GLContext myGLContext;
        ImTextureID myTexture;

        std::string myIniFileLocation;
        ImFont *myDebuggerFont;
        ImGuiSettings mySettings;
    };

} // namespace sa2
