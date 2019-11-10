#include "configuration.h"

#include "StdAfx.h"
#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Registry.h"
#include "SaveState.h"
#include "CPU.h"
#include "audio.h"

#include "linux/paddle.h"

#include "gamepadpaddle.h"

#include <QMessageBox>
#include <QGamepad>

namespace
{
    const std::vector<size_t> diskIDs = {DRIVE_1, DRIVE_2};
    const std::vector<size_t> hdIDs = {HARDDISK_1, HARDDISK_2};

    const QString REG_SCREENSHOT_TEMPLATE = QString::fromUtf8("QApple/Screenshot Template");
    const QString REG_SLOT0_CARD = QString::fromUtf8("QApple/Slot 0");
    const QString REG_RAMWORKS_SIZE = QString::fromUtf8("QApple/RamWorks Size");

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
                QMessageBox::warning(NULL, "Disk error", message);
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
            if (!HD_Insert(disk, filename.toStdString().c_str()))
            {
                const QString message = QString("Error inserting '%1'").arg(filename);
                QMessageBox::warning(NULL, "Hard Disk error", message);
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

    void setScreenshotTemplate(const QString & filenameTemplate)
    {
        QSettings().setValue(REG_SCREENSHOT_TEMPLATE, filenameTemplate);
    }

    void setSlot0Card(const int card)
    {
        QSettings().setValue(REG_SLOT0_CARD, card);
    }

    void setRamWorksMemorySize(const int memorySize)
    {
        QSettings().setValue(REG_RAMWORKS_SIZE, memorySize);
    }

}

QString getScreenshotTemplate()
{
    const QString filenameTemplate = QSettings().value(REG_SCREENSHOT_TEMPLATE, "/tmp/qapple_%1.png").toString();
    return filenameTemplate;
}

int getSlot0Card()
{
    const int slot0Card = QSettings().value(REG_SLOT0_CARD, 0).toInt();
    return slot0Card;
}

int getRamWorksMemorySize()
{
    const int ramWorksMemorySize = QSettings().value(REG_RAMWORKS_SIZE, 0).toInt();
    return ramWorksMemorySize;
}

Preferences::Data getCurrentOptions(const std::shared_ptr<QGamepad> & gamepad)
{
    Preferences::Data currentOptions;

    currentOptions.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const std::string & diskName = sg_Disk2Card.GetFullName(diskIDs[i]);
        if (!diskName.empty())
        {
            currentOptions.disks[i] = QString::fromStdString(diskName);
        }
    }

    currentOptions.hds.resize(hdIDs.size());
    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        const std::string & diskName = HD_GetFullName(hdIDs[i]);
        if (!diskName.empty())
        {
            currentOptions.hds[i] = QString::fromStdString(diskName);
        }
    }

    currentOptions.enhancedSpeed = sg_Disk2Card.GetEnhanceDisk();
    currentOptions.cardInSlot0 = getSlot0Card();
    currentOptions.mouseInSlot4 = g_Slot[4] == CT_MouseInterface;
    currentOptions.cpmInSlot5 = g_Slot[5] == CT_Z80;
    currentOptions.hdInSlot7 = HD_CardIsEnabled();
    currentOptions.ramWorksSize = getRamWorksMemorySize();

    currentOptions.apple2Type = getApple2ComputerType();

    if (gamepad)
    {
        currentOptions.joystick = gamepad->name();
        currentOptions.joystickId = gamepad->deviceId();
    }
    else
    {
        currentOptions.joystickId = 0;
    }

    const std::string & saveState = Snapshot_GetFilename();
    if (!saveState.empty())
    {
        currentOptions.saveState = QString::fromStdString(saveState);
    }

    currentOptions.screenshotTemplate = getScreenshotTemplate();

    AudioGenerator::instance().getOptions(currentOptions.audioLatency, currentOptions.silenceDelay, currentOptions.physical);

    return currentOptions;
}

void setNewOptions(const Preferences::Data & currentOptions, const Preferences::Data & newOptions, std::shared_ptr<QGamepad> & gamepad)
{
    if (currentOptions.apple2Type != newOptions.apple2Type)
    {
        const eApple2Type type = computerTypes[newOptions.apple2Type];
        SetApple2Type(type);
        REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), type);
        const eCpuType cpu = ProbeMainCpuDefault(type);
        SetMainCpu(cpu);
        REGSAVE(TEXT(REGVALUE_CPU_TYPE), cpu);
    }
    if (currentOptions.cardInSlot0 != newOptions.cardInSlot0)
    {
        setSlot0Card(newOptions.cardInSlot0);
    }
    if (currentOptions.mouseInSlot4 != newOptions.mouseInSlot4)
    {
        const SS_CARDTYPE card = newOptions.mouseInSlot4 ? CT_MouseInterface : CT_Empty;
        setSlot4(card);
    }
    if (currentOptions.cpmInSlot5 != newOptions.cpmInSlot5)
    {
        const SS_CARDTYPE card = newOptions.cpmInSlot5 ? CT_Z80 : CT_Empty;
        setSlot5(card);
    }
    if (currentOptions.hdInSlot7 != newOptions.hdInSlot7)
    {
        REGSAVE(TEXT(REGVALUE_HDD_ENABLED), newOptions.hdInSlot7 ? 1 : 0);
        HD_SetEnabled(newOptions.hdInSlot7);
    }
    if (currentOptions.ramWorksSize != newOptions.ramWorksSize)
    {
        setRamWorksMemorySize(newOptions.ramWorksSize);
    }

    if (newOptions.joystick.isEmpty())
    {
        gamepad.reset();
        Paddle::instance() = std::make_shared<Paddle>();
    }
    else
    {
        if (newOptions.joystickId != currentOptions.joystickId)
        {
            gamepad.reset(new QGamepad(newOptions.joystickId));
            Paddle::instance() = std::make_shared<GamepadPaddle>(gamepad);
        }
    }

    if (currentOptions.enhancedSpeed != newOptions.enhancedSpeed)
    {
        REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), newOptions.enhancedSpeed ? 1 : 0);
        sg_Disk2Card.SetEnhanceDisk(newOptions.enhancedSpeed);
    }

    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        if (currentOptions.disks[i] != newOptions.disks[i])
        {
            insertDisk(newOptions.disks[i], diskIDs[i]);
        }
    }

    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        if (currentOptions.hds[i] != newOptions.hds[i])
        {
            insertHD(newOptions.hds[i], hdIDs[i]);
        }
    }

    if (currentOptions.saveState != newOptions.saveState)
    {
        const std::string name = newOptions.saveState.toStdString();
        Snapshot_SetFilename(name);
        RegSaveString(TEXT(REG_CONFIG), REGVALUE_SAVESTATE_FILENAME, 1, name);
    }

    if (currentOptions.screenshotTemplate != newOptions.screenshotTemplate)
    {
        setScreenshotTemplate(newOptions.screenshotTemplate);
    }

    if (currentOptions.audioLatency != newOptions.audioLatency)
    {
        AudioGenerator::instance().setOptions(newOptions.audioLatency, newOptions.silenceDelay, newOptions.physical);
    }

}
