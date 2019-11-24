#include "configuration.h"

#include "StdAfx.h"
#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Registry.h"
#include "SaveState.h"
#include "CPU.h"
#include "audiogenerator.h"

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

Preferences::Data getCurrentPreferenceData(const std::shared_ptr<QGamepad> & gamepad)
{
    Preferences::Data currentData;

    currentData.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const std::string & diskName = sg_Disk2Card.GetFullName(diskIDs[i]);
        if (!diskName.empty())
        {
            currentData.disks[i] = QString::fromStdString(diskName);
        }
    }

    currentData.hds.resize(hdIDs.size());
    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        const std::string & diskName = HD_GetFullName(hdIDs[i]);
        if (!diskName.empty())
        {
            currentData.hds[i] = QString::fromStdString(diskName);
        }
    }

    currentData.enhancedSpeed = sg_Disk2Card.GetEnhanceDisk();
    currentData.cardInSlot0 = getSlot0Card();
    currentData.mouseInSlot4 = g_Slot[4] == CT_MouseInterface;
    currentData.cpmInSlot5 = g_Slot[5] == CT_Z80;
    currentData.hdInSlot7 = HD_CardIsEnabled();
    currentData.ramWorksSize = getRamWorksMemorySize();

    currentData.apple2Type = getApple2ComputerType();

    if (gamepad)
    {
        currentData.joystick = gamepad->name();
        currentData.joystickId = gamepad->deviceId();
    }
    else
    {
        currentData.joystickId = 0;
    }

    const std::string & saveState = Snapshot_GetFilename();
    if (!saveState.empty())
    {
        currentData.saveState = QString::fromStdString(saveState);
    }

    currentData.screenshotTemplate = getScreenshotTemplate();

    AudioGenerator::instance().getOptions(currentData.audioLatency, currentData.silenceDelay, currentData.volume);

    return currentData;
}

void setNewPreferenceData(const Preferences::Data & currentData, const Preferences::Data & newData, std::shared_ptr<QGamepad> & gamepad)
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
    if (currentData.cardInSlot0 != newData.cardInSlot0)
    {
        setSlot0Card(newData.cardInSlot0);
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
    if (currentData.ramWorksSize != newData.ramWorksSize)
    {
        setRamWorksMemorySize(newData.ramWorksSize);
    }

    if (newData.joystick.isEmpty())
    {
        gamepad.reset();
        Paddle::instance() = std::make_shared<Paddle>();
    }
    else
    {
        if (newData.joystickId != currentData.joystickId)
        {
            gamepad.reset(new QGamepad(newData.joystickId));
            Paddle::instance() = std::make_shared<GamepadPaddle>(gamepad);
        }
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

    if (currentData.screenshotTemplate != newData.screenshotTemplate)
    {
        setScreenshotTemplate(newData.screenshotTemplate);
    }

    AudioGenerator::instance().setOptions(newData.audioLatency, newData.silenceDelay, newData.volume);

}
