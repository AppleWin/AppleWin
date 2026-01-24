#pragma once

#include "frontends/sdl/sdlcompat.h"
#include "frontends/common2/gnuframe.h"
#include "frontends/common2/controllerdoublepress.h"
#include "frontends/common2/programoptions.h"
#include "linux/network/portfwds.h"

namespace sa2
{

    class SDLFrame : public common2::GNUFrame
    {
    public:
        SDLFrame(const common2::EmulatorOptions &options);

        void Begin() override;
        void End() override;

        void FrameRefreshStatus(int drawflags) override;
        int FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType) override;
        std::shared_ptr<NetworkBackend> CreateNetworkBackend(const std::string &interfaceName) override;

        // create an object to write sound output to
        std::shared_ptr<SoundBuffer> CreateSoundBuffer(
            uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName) override;

        virtual bool Quit() const = 0;

        void ProcessEvents(bool &quit);

        void FrameResetMachineState();

        const std::shared_ptr<SDL_Window> &GetWindow() const;

        void getDragDropSlotAndDrive(size_t &slot, size_t &drive) const;
        void setDragDropSlotAndDrive(const size_t slot, const size_t drive);

        bool &getPreserveAspectRatio();
        bool &getAutoBoot();

        const common2::Speed &getSpeed() const;

        void SaveSnapshot();

        static bool setGLSwapInterval(const int interval);
        static void changeGLSwapInterval(const int interval);

    protected:
        void SetApplicationIcon();
        void SetGLSynchronisation(const common2::EmulatorOptions &options);

        virtual void ProcessSingleEvent(const SDL_Event &event, bool &quit);
        virtual void GetRelativeMousePosition(const SDL_MouseMotionEvent &motion, float &x, float &y) const = 0;
        virtual void ProcessKeyDown(const SDL_KeyboardEvent &key, bool &quit);
        virtual void ToggleMouseCursor() = 0;

        void ProcessKeyUp(const SDL_KeyboardEvent &key);
        void ProcessText(const SDL_TextInputEvent &text);
        void ProcessDropEvent(const SDL_DropEvent &drop);
        void ProcessMouseButton(const SDL_MouseButtonEvent &button);
        void ProcessMouseMotion(const SDL_MouseMotionEvent &motion);

        void SetFullSpeed(const bool value) override;
        bool CanDoFullSpeed() override;

        common2::Geometry getGeometryOrDefault(const std::optional<common2::Geometry> &geometry) const;

        static float GetRelativePosition(const float value, const float size);

        int myTargetGLSwap;
        bool myPreserveAspectRatio;
        bool myForceCapsLock;
        int myMultiplier;
        bool myFullscreen;

        size_t myDragAndDropSlot;
        size_t myDragAndDropDrive;

        bool myScrollLockFullSpeed;

        std::vector<PortFwd> myPortFwds;

        std::shared_ptr<SDL_Window> myWindow;

        common2::ControllerDoublePress myControllerQuit;
    };

} // namespace sa2
