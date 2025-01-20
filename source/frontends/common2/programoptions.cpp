#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "linux/paddle.h"

#include "StdAfx.h"
#include "Log.h"
#include "Disk.h"
#include "Utilities.h"
#include "Core.h"
#include "Speaker.h"
#include "Riff.h"
#include "CardManager.h"

namespace common2
{

    EmulatorOptions::EmulatorOptions()
    {
        memclear = g_nMemoryClearType;
        configurationFile = getConfigFile("applewin.conf");
    }

    void applyOptions(const EmulatorOptions &options)
    {
        g_nMemoryClearType = options.memclear;
        g_bDisableDirectSound = options.noAudio;
        g_bDisableDirectSoundMockingboard = options.noAudio;

        LPCSTR szImageName_drive[NUM_DRIVES] = {nullptr, nullptr};
        bool driveConnected[NUM_DRIVES] = {true, true};

        if (!options.disk1.empty())
        {
            szImageName_drive[DRIVE_1] = options.disk1.c_str();
        }

        if (!options.disk2.empty())
        {
            szImageName_drive[DRIVE_2] = options.disk2.c_str();
        }

        bool bBoot = false;
        InsertFloppyDisks(SLOT6, szImageName_drive, driveConnected, bBoot);

        LPCSTR szImageName_harddisk[NUM_HARDDISKS] = {nullptr, nullptr};

        if (!options.hardDisk1.empty())
        {
            szImageName_harddisk[DRIVE_1] = options.hardDisk1.c_str();
        }

        if (!options.hardDisk2.empty())
        {
            szImageName_harddisk[DRIVE_2] = options.hardDisk2.c_str();
        }

        InsertHardDisks(SLOT7, szImageName_harddisk, bBoot);

        if (!options.customRom.empty())
        {
            CloseHandle(g_hCustomRom);
            g_hCustomRom = CreateFile(
                options.customRom.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
            if (g_hCustomRom == INVALID_HANDLE_VALUE)
            {
                LogFileOutput("Init: Failed to load Custom ROM: %s\n", options.customRom.c_str());
            }
        }

        if (!options.customRomF8.empty())
        {
            CloseHandle(g_hCustomRomF8);
            g_hCustomRomF8 = CreateFile(
                options.customRomF8.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
            if (g_hCustomRomF8 == INVALID_HANDLE_VALUE)
            {
                LogFileOutput("Init: Failed to load custom F8 ROM: %s\n", options.customRomF8.c_str());
            }
        }

        if (!options.wavFileSpeaker.empty())
        {
            if (RiffInitWriteFile(options.wavFileSpeaker.c_str(), SPKR_SAMPLE_RATE, 1))
            {
                Spkr_OutputToRiff();
            }
        }
        else if (!options.wavFileMockingboard.empty())
        {
            if (RiffInitWriteFile(options.wavFileMockingboard.c_str(), MockingboardCard::SAMPLE_RATE, 2))
            {
                GetCardMgr().GetMockingboardCardMgr().OutputToRiff();
            }
        }

        Paddle::setSquaring(options.paddleSquaring);
    }

} // namespace common2
