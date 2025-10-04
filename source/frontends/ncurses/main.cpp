#include "StdAfx.h"

#include <iostream>

#include "CardManager.h"
#include "Core.h"
#include "SaveState.h"
#include "Utilities.h"

#include "linux/benchmark.h"
#include "linux/context.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/argparser.h"
#include "frontends/common2/commoncontext.h"
#include "frontends/ncurses/world.h"
#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/evdevpaddle.h"

namespace
{

    void ProcessKeys(na2::NFrame &frame, bool &quit)
    {
        const int key = GetKeyPressed(frame);

        switch (key)
        {
        case KEY_F(2):
        {
            ResetMachineState();
            break;
        }
        case 278: // Shift-F2
        {
            CtrlReset();
            break;
        }
        case KEY_F(3):
        {
            frame.TogglePaused();
            break;
        }
        case KEY_F(4):
        case 279: // Shift-F3 (this is non standard, use F4 instead)
        {
            quit = true;
            break;
        }
        case KEY_F(5):
        {
            CardManager &cardManager = GetCardMgr();
            if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
            {
                dynamic_cast<Disk2InterfaceCard *>(cardManager.GetObj(SLOT6))->DriveSwap();
            }
            break;
        }
        case KEY_F(11):
        {
            Snapshot_SaveState();
            break;
        }
        case KEY_F(12):
        {
            frame.LoadSnapshot();
            break;
        }
        }
    }

    void ContinueExecution(const common2::EmulatorOptions &options, na2::NFrame &frame, bool &quit)
    {
        const auto start = std::chrono::steady_clock::now();

        constexpr const int64_t oneFrameMicros = 1000000 / 60; // 60 FPS
        frame.ExecuteOneFrame(oneFrameMicros);

        ProcessKeys(frame, quit);
        frame.ProcessEvDev();

        if (!options.headless)
        {
            if (g_bFullSpeed)
            {
                frame.VideoPresentScreen();
            }
            else
            {
                frame.SyncVideoPresentScreen(oneFrameMicros);
            }
        }
    }

    void EnterMessageLoop(const common2::EmulatorOptions &options, na2::NFrame &frame)
    {
        if (options.headless)
        {
            std::cerr << "Press Ctrl-C to quit." << std::endl;
        }

        bool quit = false;

        do
        {
            ContinueExecution(options, frame, quit);
        } while (!quit && !na2::g_stop);
    }

    int run_ncurses(int argc, char *const argv[])
    {
        common2::EmulatorOptions options;
        const bool run = getEmulatorOptions(argc, argv, common2::OptionsType::applen, "ncurses", options);
        options.fixedSpeed = true; // TODO: remove, some testing required
        options.syncWithTimer = true;

        if (!run)
            return 1;

        const LoggerContext loggerContext(options.log);
        const RegistryContext registryContet(CreateFileRegistry(options));
        const std::shared_ptr<na2::EvDevPaddle> paddle = std::make_shared<na2::EvDevPaddle>(options.paddleDeviceName);
        const std::shared_ptr<na2::NFrame> frame = std::make_shared<na2::NFrame>(options, paddle);

        const common2::CommonInitialisation init(frame, paddle, options);

        // no audio in the ncurses frontend
        g_bDisableDirectSound = true;
        g_bDisableDirectSoundMockingboard = true;
        na2::SetCtrlCHandler(options.headless);

        if (options.benchmark)
        {
            const auto redraw = [&frame]() { frame->VideoRedrawScreen(); };
            VideoBenchmark(redraw, redraw);
        }
        else
        {
            EnterMessageLoop(options, *frame);
        }

        return 0;
    }

} // namespace

int main(int argc, char *const argv[])
{
    try
    {
        return run_ncurses(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
}
