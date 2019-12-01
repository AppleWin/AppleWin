#include "options.h"

#include "StdAfx.h"
#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Registry.h"
#include "SaveState.h"
#include "CPU.h"
#include "Video.h"

#include <QMessageBox>
#include <QGamepad>
#include <QSettings>

namespace
{
    const std::vector<size_t> diskIDs = {DRIVE_1, DRIVE_2};
    const std::vector<size_t> hdIDs = {HARDDISK_1, HARDDISK_2};

    const QString REG_SCREENSHOT_TEMPLATE = QString::fromUtf8("QApple/Screenshot Template");
    const QString REG_SLOT0_CARD = QString::fromUtf8("QApple/Hardware/Slot 0");
    const QString REG_RAMWORKS_SIZE = QString::fromUtf8("QApple/Hardware/RamWorks Size");
    const QString REG_GAMEPAD_NAME = QString::fromUtf8("QApple/Hardware/Gamepad");
    const QString REG_AUDIO_LATENCY = QString::fromUtf8("QApple/Audio/Latency");
    const QString REG_SILENCE_DELAY = QString::fromUtf8("QApple/Audio/Silence Delay");
    const QString REG_VOLUME = QString::fromUtf8("QApple/Audio/Volume");
    const QString REG_TIMER = QString::fromUtf8("QApple/Emulator/Timer");
    const QString REG_FULL_SPEED = QString::fromUtf8("QApple/Emulator/Full Speed");

    void insertDisk(const QString & filename, const int disk)
    {
        if (filename.isEmpty())
        {
            sg_Disk2Card.EjectDisk(disk);
        }
        else
        {
            const bool createMissingDisk = true;
            const ImageError_e result = sg_Disk2Card.InsertDisk(disk, filename.toStdString().c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
            if (result != eIMAGE_ERROR_NONE)
            {
                const QString message = QString("Error [%1] inserting '%2'").arg(QString::number(result), filename);
                QMessageBox::warning(nullptr, "Disk error", message);
            }
        }
    }

    void insertHD(const QString & filename, const int disk)
    {
        if (filename.isEmpty())
        {
            HD_Unplug(disk);
        }
        else
        {
            if (!HD_Insert(disk, filename.toStdString()))
            {
                const QString message = QString("Error inserting '%1'").arg(filename);
                QMessageBox::warning(nullptr, "Hard Disk error", message);
            }
        }
    }

    void setSlot4(const SS_CARDTYPE newCardType)
    {
        g_Slot[4] = newCardType;
        REGSAVE(TEXT(REGVALUE_SLOT4), (DWORD)g_Slot[4]);
    }

    void setSlot5(const SS_CARDTYPE newCardType)
    {
        g_Slot[5] = newCardType;
        REGSAVE(TEXT(REGVALUE_SLOT5), (DWORD)g_Slot[5]);
    }

    const std::vector<eApple2Type> computerTypes = {A2TYPE_APPLE2, A2TYPE_APPLE2PLUS, A2TYPE_APPLE2E, A2TYPE_APPLE2EENHANCED,
                                                    A2TYPE_PRAVETS82, A2TYPE_PRAVETS8M, A2TYPE_PRAVETS8A, A2TYPE_TK30002E};

    int getApple2ComputerType()
    {
        const eApple2Type type = GetApple2Type();
        const auto it = std::find(computerTypes.begin(), computerTypes.end(), type);
        if (it != computerTypes.end())
        {
            return std::distance(computerTypes.begin(), it);
        }
        else
        {
            // default to A2E
            return 2;
        }
    }

}

GlobalOptions::GlobalOptions()
{
}

GlobalOptions GlobalOptions::fromQSettings()
{
    QSettings settings;
    GlobalOptions options;

    options.screenshotTemplate = settings.value(REG_SCREENSHOT_TEMPLATE, "/tmp/qapple_%1.png").toString();
    options.gamepadName = settings.value(REG_GAMEPAD_NAME, QString()).toString();
    options.slot0Card = settings.value(REG_SLOT0_CARD, 0).toInt();
    options.ramWorksMemorySize = settings.value(REG_RAMWORKS_SIZE, 0).toInt();

    options.msGap = settings.value(REG_TIMER, 5).toInt();
    options.msFullSpeed = settings.value(REG_FULL_SPEED, 5).toInt();
    options.audioLatency = settings.value(REG_AUDIO_LATENCY, 200).toInt();
    options.silenceDelay = settings.value(REG_SILENCE_DELAY, 10000).toInt();
    options.volume = settings.value(REG_VOLUME, 0x0fff).toInt();

    return options;
}

void GlobalOptions::setData(const GlobalOptions & data)
{
    if (this->msGap != data.msGap)
    {
        this->msGap = data.msGap;
        QSettings().setValue(REG_TIMER, this->msGap);
    }

    if (this->msFullSpeed != data.msFullSpeed)
    {
        this->msFullSpeed = data.msFullSpeed;
        QSettings().setValue(REG_FULL_SPEED, this->msFullSpeed);
    }

    if (this->screenshotTemplate != data.screenshotTemplate)
    {
        this->screenshotTemplate = data.screenshotTemplate;
        QSettings().setValue(REG_SCREENSHOT_TEMPLATE, this->screenshotTemplate);
    }

    if (this->slot0Card != data.slot0Card)
    {
        this->slot0Card = data.slot0Card;
        QSettings().setValue(REG_SLOT0_CARD, this->slot0Card);
    }

    if (this->ramWorksMemorySize != data.ramWorksMemorySize)
    {
        this->ramWorksMemorySize = data.ramWorksMemorySize;
        QSettings().setValue(REG_RAMWORKS_SIZE, this->ramWorksMemorySize);
    }

    if (this->gamepadName != data.gamepadName)
    {
        this->gamepadName = data.gamepadName;
        QSettings().setValue(REG_GAMEPAD_NAME, this->gamepadName);
    }

    if (this->audioLatency != data.audioLatency)
    {
        this->audioLatency = data.audioLatency;
        QSettings().setValue(REG_AUDIO_LATENCY, this->audioLatency);
    }

    if (this->silenceDelay != data.silenceDelay)
    {
        this->silenceDelay = data.silenceDelay;
        QSettings().setValue(REG_SILENCE_DELAY, this->silenceDelay);
    }

    if (this->volume != data.volume)
    {
        this->volume = data.volume;
        QSettings().setValue(REG_VOLUME, this->volume);
    }
}

void getAppleWinPreferences(PreferenceData & data)
{
    data.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const std::string & diskName = sg_Disk2Card.GetFullName(diskIDs[i]);
        if (!diskName.empty())
        {
            data.disks[i] = QString::fromStdString(diskName);
        }
    }

    data.hds.resize(hdIDs.size());
    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        const std::string & diskName = HD_GetFullName(hdIDs[i]);
        if (!diskName.empty())
        {
            data.hds[i] = QString::fromStdString(diskName);
        }
    }

    data.enhancedSpeed = sg_Disk2Card.GetEnhanceDisk();
    data.mouseInSlot4 = g_Slot[4] == CT_MouseInterface;
    data.cpmInSlot5 = g_Slot[5] == CT_Z80;
    data.hdInSlot7 = HD_CardIsEnabled();

    data.apple2Type = getApple2ComputerType();

    const std::string & saveState = Snapshot_GetFilename();
    if (!saveState.empty())
    {
        data.saveState = QString::fromStdString(saveState);
    }

    data.videoType = GetVideoType();
    data.scanLines = IsVideoStyle(VS_HALF_SCANLINES);
    data.verticalBlend = IsVideoStyle(VS_COLOR_VERTICAL_BLEND);
}

void setAppleWinPreferences(const PreferenceData & currentData, const PreferenceData & newData)
{
    if (currentData.apple2Type != newData.apple2Type)
    {
        const eApple2Type type = computerTypes[newData.apple2Type];
        SetApple2Type(type);
        REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), type);
        const eCpuType cpu = ProbeMainCpuDefault(type);
        SetMainCpu(cpu);
        REGSAVE(TEXT(REGVALUE_CPU_TYPE), cpu);
    }
    if (currentData.mouseInSlot4 != newData.mouseInSlot4)
    {
        const SS_CARDTYPE card = newData.mouseInSlot4 ? CT_MouseInterface : CT_Empty;
        setSlot4(card);
    }
    if (currentData.cpmInSlot5 != newData.cpmInSlot5)
    {
        const SS_CARDTYPE card = newData.cpmInSlot5 ? CT_Z80 : CT_Empty;
        setSlot5(card);
    }
    if (currentData.hdInSlot7 != newData.hdInSlot7)
    {
        REGSAVE(TEXT(REGVALUE_HDD_ENABLED), newData.hdInSlot7 ? 1 : 0);
        HD_SetEnabled(newData.hdInSlot7);
    }

    if (currentData.enhancedSpeed != newData.enhancedSpeed)
    {
        REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), newData.enhancedSpeed ? 1 : 0);
        sg_Disk2Card.SetEnhanceDisk(newData.enhancedSpeed);
    }

    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        if (currentData.disks[i] != newData.disks[i])
        {
            insertDisk(newData.disks[i], diskIDs[i]);
        }
    }

    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        if (currentData.hds[i] != newData.hds[i])
        {
            insertHD(newData.hds[i], hdIDs[i]);
        }
    }

    if (currentData.saveState != newData.saveState)
    {
        const std::string name = newData.saveState.toStdString();
        Snapshot_SetFilename(name);
        RegSaveString(TEXT(REG_CONFIG), REGVALUE_SAVESTATE_FILENAME, 1, name);
    }

    if (currentData.videoType != newData.videoType || currentData.scanLines != newData.scanLines || currentData.verticalBlend != newData.verticalBlend)
    {
        const VideoType_e videoType = VideoType_e(newData.videoType);
        SetVideoType(videoType);

        VideoStyle_e videoStyle = VS_NONE;
        if (newData.scanLines)
            videoStyle = VideoStyle_e(videoStyle | VS_HALF_SCANLINES);
        if (newData.verticalBlend)
            videoStyle = VideoStyle_e(videoStyle | VS_COLOR_VERTICAL_BLEND);
        SetVideoStyle(videoStyle);

        Config_Save_Video();
        VideoReinitialize();
        VideoRedrawScreen();
    }

}
