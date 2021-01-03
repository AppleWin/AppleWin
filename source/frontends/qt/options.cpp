#include "options.h"

#include "StdAfx.h"
#include "Common.h"
#include "Interface.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Registry.h"
#include "SaveState.h"
#include "CPU.h"
#include "Video.h"
#include "Speaker.h"
#include "Mockingboard.h"
#include "Configuration/IPropertySheet.h"

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
    const QString REG_GAMEPAD_NAME = QString::fromUtf8("QApple/Hardware/Gamepad/Name");
    const QString REG_GAMEPAD_SQUARING = QString::fromUtf8("QApple/Hardware/Gamepad/Squaring");
    const QString REG_AUDIO_LATENCY = QString::fromUtf8("QApple/Audio/Latency");
    const QString REG_SILENCE_DELAY = QString::fromUtf8("QApple/Audio/Silence Delay");
    const QString REG_VOLUME = QString::fromUtf8("QApple/Audio/Volume");
    const QString REG_TIMER = QString::fromUtf8("QApple/Emulator/Timer");
    const QString REG_FULL_SPEED = QString::fromUtf8("QApple/Emulator/Full Speed");

    void insertDisk(const QString & filename, const int disk)
    {
        CardManager & cardManager = GetCardMgr();
        if (cardManager.QuerySlot(SLOT6) != CT_Disk2)
            return;

        Disk2InterfaceCard* pDisk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));
        if (filename.isEmpty())
        {
            pDisk2Card->EjectDisk(disk);
        }
        else
        {
            const bool createMissingDisk = true;
            const ImageError_e result = pDisk2Card->InsertDisk(disk, filename.toStdString().c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
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

    void SetSlot(UINT slot, SS_CARDTYPE newCardType)
    {
        _ASSERT(slot < NUM_SLOTS);
        if (slot >= NUM_SLOTS)
            return;

        CardManager & cardManager = GetCardMgr();

        // Two paths:
        // 1) Via Config dialog: card not inserted yet
        // 2) Snapshot_LoadState_v2(): card already inserted
        if (cardManager.QuerySlot(slot) != newCardType)
            cardManager.Insert(slot, newCardType);

        std::string slotText;
        switch (slot)
        {
        case 0: slotText = REGVALUE_SLOT0; break;
        case 1: slotText = REGVALUE_SLOT1; break;
        case 2: slotText = REGVALUE_SLOT2; break;
        case 3: slotText = REGVALUE_SLOT3; break;
        case 4: slotText = REGVALUE_SLOT4; break;
        case 5: slotText = REGVALUE_SLOT5; break;
        case 6: slotText = REGVALUE_SLOT6; break;
        case 7: slotText = REGVALUE_SLOT7; break;
        }

        REGSAVE(slotText.c_str(), (DWORD)newCardType);
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
    options.gamepadSquaring = settings.value(REG_GAMEPAD_SQUARING, true).toBool();
    options.slot0Card = settings.value(REG_SLOT0_CARD, 0).toInt();
    options.ramWorksMemorySize = settings.value(REG_RAMWORKS_SIZE, 0).toInt();

    options.msGap = settings.value(REG_TIMER, 5).toInt();
    options.msFullSpeed = settings.value(REG_FULL_SPEED, 5).toInt();
    options.audioLatency = settings.value(REG_AUDIO_LATENCY, 200).toInt();

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

    if (this->gamepadSquaring != data.gamepadSquaring)
    {
        this->gamepadSquaring = data.gamepadSquaring;
        QSettings().setValue(REG_GAMEPAD_SQUARING, this->gamepadSquaring);
    }

    if (this->audioLatency != data.audioLatency)
    {
        this->audioLatency = data.audioLatency;
        QSettings().setValue(REG_AUDIO_LATENCY, this->audioLatency);
    }

}

void getAppleWinPreferences(PreferenceData & data)
{
    CardManager & cardManager = GetCardMgr();

    Disk2InterfaceCard* pDisk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));

    data.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const std::string & diskName = pDisk2Card->GetFullName(diskIDs[i]);
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

    data.enhancedSpeed = pDisk2Card->GetEnhanceDisk();
    data.cardInSlot4 = cardManager.QuerySlot(SLOT4);
    data.cardInSlot5 = cardManager.QuerySlot(SLOT5);
    data.hdInSlot7 = HD_CardIsEnabled();

    data.apple2Type = GetApple2Type();

    data.speakerVolume = SpkrGetVolume();
    data.mockingboardVolume = MB_GetVolume();

    const std::string & saveState = Snapshot_GetFilename();
    if (!saveState.empty())
    {
        data.saveState = QString::fromStdString(saveState);
    }

    Video & video = GetVideo();

    data.videoType = video.GetVideoType();
    data.scanLines = video.IsVideoStyle(VS_HALF_SCANLINES);
    data.verticalBlend = video.IsVideoStyle(VS_COLOR_VERTICAL_BLEND);
    data.hz50 = video.GetVideoRefreshRate() == VR_50HZ;
    data.monochromeColor.setRgb(video.GetMonochromeRGB());
}

void setAppleWinPreferences(const PreferenceData & currentData, const PreferenceData & newData)
{
    CardManager & cardManager = GetCardMgr();
    Disk2InterfaceCard* pDisk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));

    if (currentData.speakerVolume != newData.speakerVolume)
    {
        SpkrSetVolume(newData.speakerVolume, GetPropertySheet().GetVolumeMax());
        REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
    }

    if (currentData.mockingboardVolume != newData.mockingboardVolume)
    {
        MB_SetVolume(newData.mockingboardVolume, GetPropertySheet().GetVolumeMax());
        REGSAVE(TEXT(REGVALUE_MB_VOLUME), MB_GetVolume());
    }

    if (currentData.apple2Type != newData.apple2Type)
    {
        SetApple2Type(newData.apple2Type);
        REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), newData.apple2Type);
        const eCpuType cpu = ProbeMainCpuDefault(newData.apple2Type);
        SetMainCpu(cpu);
        REGSAVE(TEXT(REGVALUE_CPU_TYPE), cpu);
    }
    if (currentData.cardInSlot4 != newData.cardInSlot4)
    {
        SetSlot(SLOT4, newData.cardInSlot4);
    }
    if (currentData.cardInSlot5 != newData.cardInSlot5)
    {
        SetSlot(SLOT5, newData.cardInSlot5);
    }
    if (currentData.hdInSlot7 != newData.hdInSlot7)
    {
        REGSAVE(TEXT(REGVALUE_HDD_ENABLED), newData.hdInSlot7 ? 1 : 0);
        HD_SetEnabled(newData.hdInSlot7);
    }

    if (currentData.enhancedSpeed != newData.enhancedSpeed)
    {
        REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), newData.enhancedSpeed ? 1 : 0);
        pDisk2Card->SetEnhanceDisk(newData.enhancedSpeed);
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

    if (currentData.videoType != newData.videoType || currentData.scanLines != newData.scanLines || currentData.verticalBlend != newData.verticalBlend
            || currentData.hz50 != newData.hz50 || currentData.monochromeColor != newData.monochromeColor)
    {
        Video & video = GetVideo();

        const VideoType_e videoType = VideoType_e(newData.videoType);
        video.SetVideoType(videoType);

        VideoStyle_e videoStyle = VS_NONE;
        if (newData.scanLines)
            videoStyle = VideoStyle_e(videoStyle | VS_HALF_SCANLINES);
        if (newData.verticalBlend)
            videoStyle = VideoStyle_e(videoStyle | VS_COLOR_VERTICAL_BLEND);
        video.SetVideoStyle(videoStyle);

        const VideoRefreshRate_e videoRefreshRate = newData.hz50 ? VR_50HZ : VR_60HZ;
        video.SetVideoRefreshRate(videoRefreshRate);

        const QColor color = newData.monochromeColor;
        // be careful QRgb is opposite way round to COLORREF
        video.SetMonochromeRGB(RGB(color.red(), color.green(), color.blue()));

        video.Config_Save_Video();
        video.VideoReinitialize();
        video.VideoRedrawScreen();
    }

}
