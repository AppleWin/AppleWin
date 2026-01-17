#include "StdAfx.h"
#include "frontends/sdl/processfile.h"
#include "frontends/sdl/sdlframe.h"
#include "frontends/sdl/utils.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"

#include "CardManager.h"
#include "Core.h"
#include "Utilities.h"
#include "SaveState.h"
#include "SoundCore.h"
#include "Interface.h"
#include "MouseInterface.h"
#include "Debugger/Debug.h"

#include "../resource/resource.h"
#include "linux/paddle.h"
#include "linux/keyboardbuffer.h"
#include "linux/network/slirp2.h"

#include <SDL_image.h>

// #define KEY_LOGGING_VERBOSE

#ifdef KEY_LOGGING_VERBOSE
#include "Log.h"
#endif

namespace
{

    void processAppleKey(const SDL_KeyboardEvent &key, const bool forceCapsLock)
    {
        // using keycode (or scan code) one takes a physical view of the keyboard
        // ignoring non US layouts
        // SHIFT-3 on a A2 keyboard in # while on my UK keyboard is £?
        // so for now all ASCII keys are handled as text below
        // but this makes it impossible to detect CTRL-ASCII... more to follow
        BYTE ch = 0;

        switch (key.keysym.sym)
        {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        {
            ch = 0x0d;
            break;
        }
#ifndef __APPLE__
        // On macOS, the [delete] key emits SDLK_BACKSPACE but we want to map it to DEL (0x7F)
        case SDLK_BACKSPACE: // same as AppleWin
#endif
        case SDLK_LEFT:
        {
            ch = 0x08;
            break;
        }
        case SDLK_RIGHT:
        {
            ch = 0x15;
            break;
        }
        case SDLK_UP:
        {
            ch = 0x0b;
            break;
        }
        case SDLK_DOWN:
        {
            ch = 0x0a;
            break;
        }
#ifdef __APPLE__
        // On macOS, the [delete] key emits SDLK_BACKSPACE but we want to map it to DEL (0x7F)
        case SDLK_BACKSPACE:
#endif
        case SDLK_DELETE:
        {
            ch = 0x7f;
            break;
        }
        case SDLK_ESCAPE:
        {
            ch = 0x1b;
            break;
        }
        case SDLK_TAB:
        {
            ch = 0x09;
            break;
        }
        case SDLK_a:
        case SDLK_b:
        case SDLK_c:
        case SDLK_d:
        case SDLK_e:
        case SDLK_f:
        case SDLK_g:
        case SDLK_h:
        case SDLK_i:
        case SDLK_j:
        case SDLK_k:
        case SDLK_l:
        case SDLK_m:
        case SDLK_n:
        case SDLK_o:
        case SDLK_p:
        case SDLK_q:
        case SDLK_r:
        case SDLK_s:
        case SDLK_t:
        case SDLK_u:
        case SDLK_v:
        case SDLK_w:
        case SDLK_x:
        case SDLK_y:
        case SDLK_z:
        {
            // same logic as AW
            // CAPS is forced when the emulator starts
            // until is used the first time
            const bool capsLock = forceCapsLock || (SDL_GetModState() & KMOD_CAPS);
            const bool upper = capsLock || (key.keysym.mod & KMOD_SHIFT);

            ch = (key.keysym.sym - SDLK_a) + 0x01;
            if (key.keysym.mod & KMOD_CTRL)
            {
                // ok
            }
            else if (upper)
            {
                ch += 0x40; // upper case
            }
            else
            {
                ch += 0x60; // lower case
            }
            break;
        }
        }

        if (ch)
        {
            addKeyToBuffer(ch);
#ifdef KEY_LOGGING_VERBOSE
            LogOutput("SDL KeyboardEvent: %02x\n", ch);
#endif
        }
    }

} // namespace

namespace sa2
{

    SDLFrame::SDLFrame(const common2::EmulatorOptions &options)
        : common2::GNUFrame(options)
        , myTargetGLSwap(options.glSwapInterval)
        , myPreserveAspectRatio(options.aspectRatio)
        , myForceCapsLock(true)
        , myMultiplier(1)
        , myFullscreen(false)
        , myDragAndDropSlot(SLOT6)
        , myDragAndDropDrive(DRIVE_1)
        , myScrollLockFullSpeed(false)
        , myPortFwds(getPortFwds(options.natPortFwds))
    {
    }

    void SDLFrame::SetGLSynchronisation(const common2::EmulatorOptions &options)
    {
        const int defaultGLSwap = SDL_GL_GetSwapInterval();
        if (defaultGLSwap == 0)
        {
            // sane default
            mySynchroniseWithTimer = true;
            myTargetGLSwap = 0;
        }
        else if (options.syncWithTimer)
        {
            mySynchroniseWithTimer = true;
            myTargetGLSwap = 0;
        }
        else
        {
            mySynchroniseWithTimer = false;
            myTargetGLSwap = options.glSwapInterval;
        }
    }

    void SDLFrame::End()
    {
        CommonFrame::End();
        if (!myFullscreen)
        {
            common2::Geometry geometry;
            SDL_GetWindowPosition(myWindow.get(), &geometry.x, &geometry.y);
            SDL_GetWindowSize(myWindow.get(), &geometry.width, &geometry.height);
            saveGeometryToRegistry("sa2", geometry);
        }
    }

    void SDLFrame::setGLSwapInterval(const int interval)
    {
        const int current = SDL_GL_GetSwapInterval();
        // in QEMU with GL_RENDERER: llvmpipe (LLVM 12.0.0, 256 bits)
        // SDL_GL_SetSwapInterval() always fails
        if (interval != current && SDL_GL_SetSwapInterval(interval))
        {
            throw std::runtime_error(decorateSDLError("SDL_GL_SetSwapInterval"));
        }
    }

    void SDLFrame::Begin()
    {
        CommonFrame::Begin();
        setGLSwapInterval(myTargetGLSwap);
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
        const auto resource = GetResourceData(IDC_APPLEWIN_ICON);

        SDL_RWops *ops = SDL_RWFromConstMem(resource.first, resource.second);

        std::shared_ptr<SDL_Surface> icon(IMG_Load_RW(ops, 1), SDL_FreeSurface);
        if (icon)
        {
            SDL_SetWindowIcon(myWindow.get(), icon.get());
        }
    }

    const std::shared_ptr<SDL_Window> &SDLFrame::GetWindow() const
    {
        return myWindow;
    }

    int SDLFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
    {
        // tabs do not render properly
        std::string s(lpText);
        std::replace(s.begin(), s.end(), '\t', ' ');
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, lpCaption, s.c_str(), nullptr);
        ResetSpeed();
        return IDOK;
    }

    void SDLFrame::ProcessEvents(bool &quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0)
        {
            ProcessSingleEvent(e, quit);
        }
    }

    void SDLFrame::ProcessSingleEvent(const SDL_Event &e, bool &quit)
    {
        switch (e.type)
        {
        case SDL_QUIT:
        {
            quit = true;
            break;
        }
        case SDL_KEYDOWN:
        {
            ProcessKeyDown(e.key, quit);
            break;
        }
        case SDL_KEYUP:
        {
            ProcessKeyUp(e.key);
            break;
        }
        case SDL_TEXTINPUT:
        {
            ProcessText(e.text);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            ProcessMouseButton(e.button);
            break;
        }
        case SDL_MOUSEMOTION:
        {
            ProcessMouseMotion(e.motion);
            break;
        }
        case SDL_DROPFILE:
        {
            ProcessDropEvent(e.drop);
            SDL_free(e.drop.file);
            break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
        {
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) // SELECT
            {
                quit = myControllerQuit.pressButton();
            }
            break;
        }
        }
    }

    void SDLFrame::ProcessMouseButton(const SDL_MouseButtonEvent &event)
    {
        CardManager &cardManager = GetCardMgr();

        if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
        {
            switch (event.button)
            {
            case SDL_BUTTON_LEFT:
            case SDL_BUTTON_RIGHT:
            {
                const eBUTTONSTATE state = (event.state == SDL_PRESSED) ? BUTTON_DOWN : BUTTON_UP;
                const eBUTTON button = (event.button == SDL_BUTTON_LEFT) ? BUTTON0 : BUTTON1;
                cardManager.GetMouseCard()->SetButton(button, state);
                break;
            }
            }
        }
    }

    float SDLFrame::GetRelativePosition(const float value, const float size)
    {
        // the minimum size of a window is 1
        const float result = value / std::max(1.0f, size - 1.0f);
        return result;
    }

    void SDLFrame::ProcessMouseMotion(const SDL_MouseMotionEvent &motion)
    {
        CardManager &cardManager = GetCardMgr();

        if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
        {
            int iX, iMinX, iMaxX;
            int iY, iMinY, iMaxY;
            cardManager.GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

            int width, height;
            SDL_GetWindowSize(myWindow.get(), &width, &height);
            const bool relative = (iMaxX - iMinX == 32767) && (iMaxY - iMinY == 32767);

            int dx, dy;
            if (relative)
            {
                // in geos the screen in 280 * 2 mouse ticks in x and 192 in y
                // everything works in relative motion
                // so we do not know where the pointer is
                //
                // the risk is that we leave the SDL window before the guest pointer reaches the end
                // and we stop receiving events
                //
                // we could call SDL_CaptureMouse(), but this interacts badly with ImGui
                //
                // so we make the guest pointer a bit faster (alpha > 1)
                //
                // at least the pointer moves at the right speed
                // and with some gymnic, one can put it in the right place

                const double sizeX = 280 * 2;
                const double sizeY = 192;

                const double relX = double(motion.xrel) / double(width) * sizeX;
                const double relY = double(motion.yrel) / double(height) * sizeY;

                const double alpha = 1.1;
                dx = lround(relX * alpha);
                dy = lround(relY * alpha);
            }
            else
            {
                const int sizeX = iMaxX - iMinX;
                const int sizeY = iMaxY - iMinY;

                float x, y;
                GetRelativeMousePosition(motion, x, y);

                const int newX = lround(x * sizeX) + iMinX;
                const int newY = lround(y * sizeY) + iMinY;

                dx = newX - iX;
                dy = newY - iY;
            }

            int outOfBoundsX;
            int outOfBoundsY;
            cardManager.GetMouseCard()->SetPositionRel(dx, dy, &outOfBoundsX, &outOfBoundsY);
        }
    }

    void SDLFrame::ProcessDropEvent(const SDL_DropEvent &drop)
    {
        processFile(this, drop.file, myDragAndDropSlot, myDragAndDropDrive);
    }

    void SDLFrame::ProcessKeyDown(const SDL_KeyboardEvent &key, bool &quit)
    {
        // scancode vs keycode
        // scancode is relative to the position on the keyboard
        // keycode is what the os maps it to
        // if the user has decided to change the layout, we just go with it and use the keycode
        if (!key.repeat)
        {
            const size_t modifiers = getCanonicalModifiers(key);
            switch (key.keysym.sym)
            {
            case SDLK_F12:
            {
                if (modifiers == KMOD_NONE)
                {
                    LoadSnapshot();
                }
                break;
            }
            case SDLK_F11:
            {
                if (modifiers == KMOD_NONE)
                {
                    SaveSnapshot();
                }
                break;
            }
            case SDLK_F9:
            {
                if (modifiers == KMOD_NONE)
                {
                    CycleVideoType();
                }
                else if (modifiers == KMOD_CTRL)
                {
                    ToggleMouseCursor();
                }
                break;
            }
            case SDLK_F6:
            {
                if (modifiers == (KMOD_CTRL | KMOD_SHIFT))
                {
                    Cycle50ScanLines();
                }
                else if (modifiers == KMOD_CTRL)
                {
                    Video &video = GetVideo();
                    myMultiplier = myMultiplier == 1 ? 2 : 1;
                    const int sw = video.GetFrameBufferBorderlessWidth();
                    const int sh = video.GetFrameBufferBorderlessHeight();
                    SDL_SetWindowSize(myWindow.get(), sw * myMultiplier, sh * myMultiplier);
                }
                else if (modifiers == KMOD_NONE)
                {
                    myFullscreen = !myFullscreen;
                    SDL_SetWindowFullscreen(myWindow.get(), myFullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                }
                break;
            }
            case SDLK_F5:
            {
                if (modifiers == KMOD_NONE)
                {
                    CardManager &cardManager = GetCardMgr();
                    if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
                    {
                        dynamic_cast<Disk2InterfaceCard *>(cardManager.GetObj(SLOT6))->DriveSwap();
                    }
                }
                break;
            }
            case SDLK_F4:
            {
                if (modifiers == KMOD_ALT)
                {
                    // In GNOME (and probably most Desktop Environments)
                    // we never get here: we receive instead a SDL_QUIT
                    // this is only a fallback for "other" environments
                    quit = true;
                }
                break;
            }
            case SDLK_F2:
            {
                if (modifiers == KMOD_CTRL)
                {
                    CtrlReset();
                }
                else if (modifiers == KMOD_SHIFT)
                {
                    quit = true;
                }
                else if (modifiers == KMOD_NONE)
                {
                    FrameResetMachineState();
                }
                break;
            }
            case SDLK_F1:
            {
                if (modifiers == KMOD_CTRL)
                {
                    sa2::printAudioInfo();
                }
                break;
            }
            case SDLK_LALT:
            {
                Paddle::setButtonPressed(Paddle::ourOpenApple);
                break;
            }
            case SDLK_RALT:
            {
                Paddle::setButtonPressed(Paddle::ourSolidApple);
                break;
            }
            case SDLK_PAUSE:
            {
                TogglePaused();
                break;
            }
            case SDLK_CAPSLOCK:
            {
                myForceCapsLock = false;
                break;
            }
            case SDLK_INSERT:
            {
                if (modifiers == (KMOD_CTRL | KMOD_SHIFT))
                {
                    Video_TakeScreenShot(Video::SCREENSHOT_560x384);
                }
                else if (modifiers == KMOD_SHIFT)
                {
                    char *text = SDL_GetClipboardText();
                    if (text)
                    {
                        addTextToBuffer(text);
                        SDL_free(text);
                    }
                }
                else if (modifiers == KMOD_CTRL)
                {
                    // in AppleWin this is Ctrl-PrintScreen
                    // but PrintScreen is not passed to SDL
                    char *pText;
                    const size_t size = Util_GetTextScreen(pText);
                    if (size)
                    {
                        SDL_SetClipboardText(pText);
                    }
                }
                break;
            }
            case SDLK_SCROLLLOCK:
            {
                myScrollLockFullSpeed = !myScrollLockFullSpeed;
                break;
            }
            }
        }

        processAppleKey(key, myForceCapsLock);
    }

    void SDLFrame::ProcessKeyUp(const SDL_KeyboardEvent &key)
    {
        switch (key.keysym.sym)
        {
        case SDLK_LALT:
        {
            Paddle::setButtonReleased(Paddle::ourOpenApple);
            break;
        }
        case SDLK_RALT:
        {
            Paddle::setButtonReleased(Paddle::ourSolidApple);
            break;
        }
        }
    }

    void SDLFrame::ProcessText(const SDL_TextInputEvent &text)
    {
        if (strlen(text.text) == 1)
        {
            const char key = text.text[0];
            switch (key)
            {
            // 0x20 ... 0x40
            case 0x20:
            case 0x21:
            case 0x22:
            case 0x23:
            case 0x24:
            case 0x25:
            case 0x26:
            case 0x27:
            case 0x28:
            case 0x29:
            case 0x2A:
            case 0x2B:
            case 0x2C:
            case 0x2D:
            case 0x2E:
            case 0x2F:
            case 0x30:
            case 0x31:
            case 0x32:
            case 0x33:
            case 0x34:
            case 0x35:
            case 0x36:
            case 0x37:
            case 0x38:
            case 0x39:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
            case 0x40:

            // 0x5B ... 0x60
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x5E:
            case 0x5F:
            case 0x60:

            // 0x7B ... 0x7E
            case 0x7B:
            case 0x7C:
            case 0x7D:
            case 0x7E:
            {
                // not the letters
                // this is very simple, but one cannot handle CRTL-key combination.
                addKeyToBuffer(key);
#ifdef KEY_LOGGING_VERBOSE
                LogOutput("SDL TextInputEvent: %02x\n", key);
#endif
                break;
            }
            }
        }
    }

    void SDLFrame::getDragDropSlotAndDrive(size_t &slot, size_t &drive) const
    {
        slot = myDragAndDropSlot;
        drive = myDragAndDropDrive;
    }

    void SDLFrame::setDragDropSlotAndDrive(const size_t slot, const size_t drive)
    {
        myDragAndDropSlot = slot;
        myDragAndDropDrive = drive;
    }

    bool &SDLFrame::getPreserveAspectRatio()
    {
        return myPreserveAspectRatio;
    }

    bool &SDLFrame::getAutoBoot()
    {
        return myAutoBoot;
    }

    void SDLFrame::SetFullSpeed(const bool value)
    {
        CommonFrame::SetFullSpeed(value);
        if (g_bFullSpeed != value)
        {
            if (value)
            {
                // entering full speed
                setGLSwapInterval(0);
            }
            else
            {
                // leaving full speed
                setGLSwapInterval(myTargetGLSwap);
            }
        }
    }

    bool SDLFrame::CanDoFullSpeed()
    {
        return myScrollLockFullSpeed || CommonFrame::CanDoFullSpeed();
    }

    void SDLFrame::FrameResetMachineState()
    {
        ResetMachineState(); // this changes g_bFullSpeed
        ResetSpeed();
    }

    common2::Geometry SDLFrame::getGeometryOrDefault(const std::optional<common2::Geometry> &geometry) const
    {
        if (geometry)
        {
            return *geometry;
        }

        Video &video = GetVideo();
        const int sw = video.GetFrameBufferBorderlessWidth();
        const int sh = video.GetFrameBufferBorderlessHeight();

        // initialise with defaults
        common2::Geometry actual = {sw * 2, sh * 2, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED};

        // add registry information
        loadGeometryFromRegistry("sa2", actual);

        return actual;
    }

    std::shared_ptr<NetworkBackend> SDLFrame::CreateNetworkBackend(const std::string &interfaceName)
    {
#ifdef U2_USE_SLIRP
        return std::make_shared<SlirpBackend>(myPortFwds);
#else
        return common2::GNUFrame::CreateNetworkBackend(interfaceName);
#endif
    }

    const common2::Speed &SDLFrame::getSpeed() const
    {
        return mySpeed;
    }

    void SDLFrame::SaveSnapshot()
    {
        const std::string &pathname = Snapshot_GetPathname();
        const std::string message = "Do you want to save the state to: " + pathname + "?";
        SoundCore_SetFade(FADE_OUT);
        if (show_yes_no_dialog(myWindow, "Save state", message))
        {
            Snapshot_SaveState();
        }
        SoundCore_SetFade(FADE_IN);
        ResetSpeed();
    }

    std::shared_ptr<SoundBuffer> SDLFrame::CreateSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName)
    {
        const auto buffer = iCreateDirectSoundBuffer(dwBufferSize, nSampleRate, nChannels, pszVoiceName);
        return buffer;
    }

} // namespace sa2
