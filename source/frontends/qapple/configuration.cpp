#include "configuration.h"

#include "StdAfx.h"
#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Registry.h"
#include "SaveState.h"
#include "CPU.h"

#include "linux/paddle.h"

#include "gamepadpaddle.h"

#include <QMessageBox>
#include <QGamepad>

namespace
{
    const std::vector<size_t> diskIDs = {DRIVE_1, DRIVE_2};
    const std::vector<size_t> hdIDs = {HARDDISK_1, HARDDISK_2};

    void insertDisk(const QString & filename, const int disk)
    {
        if (filename.isEmpty())
        {
            DiskEject(disk);
        }
        else
        {
            const bool createMissingDisk = true;
            const ImageError_e result = DiskInsert(disk, filename.toStdString().c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
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

    void setSlot0(const SS_CARDTYPE newCardType)
    {
        g_Slot0 = newCardType;
        REGSAVE(TEXT(REGVALUE_SLOT0), (DWORD)g_Slot0);
    }

    void setSlot4(const SS_CARDTYPE newCardType)
    {
        g_Slot4 = newCardType;
        REGSAVE(TEXT(REGVALUE_SLOT4), (DWORD)g_Slot4);
    }

    void setSlot5(const SS_CARDTYPE newCardType)
    {
        g_Slot5 = newCardType;
        REGSAVE(TEXT(REGVALUE_SLOT5), (DWORD)g_Slot5);
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
        QSettings().setValue("QApple/Screenshot Template", filenameTemplate);
    }

}

QString getScreenshotTemplate()
{
    const QString filenameTemplate = QSettings().value("QApple/Screenshot Template", "/tmp/qapple_%1.png").toString();
    return filenameTemplate;
}

Preferences::Data getCurrentOptions(const std::shared_ptr<QGamepad> & gamepad)
{
    Preferences::Data currentOptions;

    currentOptions.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const char * diskName = DiskGetFullName(diskIDs[i]);
        if (diskName)
        {
            currentOptions.disks[i] = diskName;
        }
    }

    currentOptions.hds.resize(hdIDs.size());
    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        const char * diskName = HD_GetFullName(hdIDs[i]);
        if (diskName)
        {
            currentOptions.hds[i] = diskName;
        }
    }

    currentOptions.languageCardInSlot0 = g_Slot0 == CT_LanguageCard;
    currentOptions.mouseInSlot4 = g_Slot4 == CT_MouseInterface;
    currentOptions.cpmInSlot5 = g_Slot5 == CT_Z80;
    currentOptions.hdInSlot7 = HD_CardIsEnabled();

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

    const char* saveState = Snapshot_GetFilename();
    if (saveState)
    {
        currentOptions.saveState = QString::fromUtf8(saveState);;
    }

    currentOptions.screenshotTemplate = getScreenshotTemplate();

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
    if (currentOptions.languageCardInSlot0 != newOptions.languageCardInSlot0)
    {
        const SS_CARDTYPE card = newOptions.languageCardInSlot0 ? CT_LanguageCard : CT_Empty;
        setSlot0(card);
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
        RegSaveString(TEXT(REG_CONFIG), REGVALUE_SAVESTATE_FILENAME, 1, name.c_str());
    }

    if (currentOptions.screenshotTemplate != newOptions.screenshotTemplate)
    {
        setScreenshotTemplate(newOptions.screenshotTemplate);
    }

}
