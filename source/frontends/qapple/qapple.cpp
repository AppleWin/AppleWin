#include "qapple.h"

#include "StdAfx.h"
#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "ParallelPrinter.h"
#include "Video.h"
#include "SaveState.h"
#include "Registry.h"

#include "linux/data.h"
#include "linux/benchmark.h"
#include "linux/paddle.h"

#include "emulator.h"
#include "memorycontainer.h"
#include "gamepadpaddle.h"
#include "settings.h"

#include <QMdiSubWindow>
#include <QMessageBox>
#include <QFileDialog>

namespace
{

    void initialiseEmulator()
    {
        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, NULL);

        LogFileOutput("Initialisation\n");

        ImageInitialize();
        DiskInitialize();
    }

    void startEmulator(QWidget * window)
    {
        LoadConfiguration();

        CheckCpu();

        SetWindowTitle();
        window->setWindowTitle(g_pAppTitle);

        FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

        MemInitialize();
        VideoInitialize();
    }

    void stopEmulator()
    {
        MemDestroy();
    }

    void uninitialiseEmulator()
    {
        HD_Destroy();
        PrintDestroy();
        CpuDestroy();
        MemDestroy();

        DiskDestroy();
    }

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

    QString getScreenshotTemplate()
    {
        const QString filenameTemplate = QSettings().value("QApple/Screenshot Template", "/tmp/qapple_%1.png").toString();
        return filenameTemplate;
    }

    void setScreenshotTemplate(const QString & filenameTemplate)
    {
        QSettings().setValue("QApple/Screenshot Template", filenameTemplate);
    }
}

void FrameDrawDiskLEDS(HDC)
{
}

void FrameDrawDiskStatus(HDC)
{

}

void FrameRefreshStatus(int, bool)
{

}

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
    Q_UNUSED(pc)
    Q_UNUSED(addr)
    Q_UNUSED(bWrite)
    Q_UNUSED(d)
    Q_UNUSED(nCyclesLeft)
    return 0;
}

void VideoInitialize() {}

// MessageBox

int MessageBox(HWND, const char * text, const char * caption, UINT type)
{
    QMessageBox::StandardButtons buttons = QMessageBox::Ok;
    if (type & MB_YESNO)
    {
        buttons = QMessageBox::Yes | QMessageBox::No;
    }
    else if (type & MB_YESNOCANCEL)
    {
        buttons = QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel;
    }

    QMessageBox::StandardButton result = QMessageBox::information(nullptr, caption, text, buttons);

    switch (result)
    {
    case QMessageBox::Ok:
        return IDOK;
    case QMessageBox::Yes:
        return IDYES;
    case QMessageBox::No:
        return IDNO;
    case QMessageBox::Cancel:
        return IDCANCEL;
    default:
        return IDOK;
    }
}

QApple::QApple(QWidget *parent) :
    QMainWindow(parent), myTimerID(0), myPreferences(this)
{
    setupUi(this);

    actionStart->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    actionPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    actionReboot->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));

    actionSave_state->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    actionLoad_state->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));

    myEmulator = new Emulator(mdiArea);
    myEmulatorWindow = mdiArea->addSubWindow(myEmulator, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    myMSGap = 5;

    on_actionPause_triggered();
    initialiseEmulator();
    startEmulator(myEmulatorWindow);
}

void QApple::closeEvent(QCloseEvent *)
{
    stopTimer();
    uninitialiseEmulator();
}

void QApple::on_timer()
{
    const qint64 elapsed = myElapsedTimer.restart();
    const double fUsecPerSec        = 1.e6;
    const UINT nExecutionPeriodUsec = 1000 * elapsed;

    const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
    const DWORD uCyclesToExecute = fExecutionPeriodClks;

    const bool bVideoUpdate = false;

    do
    {
        const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
        g_dwCyclesThisFrame += uActualCyclesExecuted;
        if (g_dwCyclesThisFrame >= dwClksPerFrame)
        {
            g_dwCyclesThisFrame -= dwClksPerFrame;
            myEmulator->updateVideo();
        }
    }
    while (DiskIsSpinning());
}

void QApple::stopTimer()
{
    if (myTimerID)
    {
        killTimer(myTimerID);
        myTimerID = 0;
    }
}

void QApple::on_actionStart_triggered()
{
    // always restart with the same timer gap that was last used
    myTimerID = startTimer(myMSGap, Qt::PreciseTimer);
    myElapsedTimer.start();
    actionPause->setEnabled(true);
    actionStart->setEnabled(false);
}

void QApple::on_actionPause_triggered()
{
    stopTimer();
    actionPause->setEnabled(false);
    actionStart->setEnabled(true);
}

void QApple::on_actionX1_triggered()
{
    myEmulator->setZoom(myEmulatorWindow, 1);
}

void QApple::on_actionX2_triggered()
{
    myEmulator->setZoom(myEmulatorWindow, 2);
}

void QApple::on_action4_3_triggered()
{
    myEmulator->set43AspectRatio(myEmulatorWindow);
}

void QApple::on_actionReboot_triggered()
{
    emit endEmulator();
    stopEmulator();
    startEmulator(myEmulatorWindow);
    myEmulatorWindow->setWindowTitle(g_pAppTitle);
}

void QApple::on_actionBenchmark_triggered()
{
    // call repaint as we really want to for a paintEvent() so we can time it properly
    // if video is based on OpenGLWidget, this is not enough though,
    // and benchmark results are bad.
    VideoBenchmark([this]() { myEmulator->repaintVideo(); });
    on_actionReboot_triggered();
}

void QApple::timerEvent(QTimerEvent *)
{
    on_timer();
}

void QApple::on_actionMemory_triggered()
{
    MemoryContainer * container = new MemoryContainer(mdiArea);
    QMdiSubWindow * window = mdiArea->addSubWindow(container);

    // need to close as it points to old memory
    connect(this, SIGNAL(endEmulator()), window, SLOT(close()));

    window->setWindowTitle("Memory viewer");
    window->show();
}

void QApple::on_actionOptions_triggered()
{
    Preferences::Data currentOptions;

    const std::vector<size_t> diskIDs = {DRIVE_1, DRIVE_2};
    currentOptions.disks.resize(diskIDs.size());
    for (size_t i = 0; i < diskIDs.size(); ++i)
    {
        const char * diskName = DiskGetFullName(diskIDs[i]);
        if (diskName)
        {
            currentOptions.disks[i] = diskName;
        }
    }

    const std::vector<size_t> hdIDs = {HARDDISK_1, HARDDISK_2};
    currentOptions.hds.resize(hdIDs.size());
    for (size_t i = 0; i < hdIDs.size(); ++i)
    {
        const char * diskName = HD_GetFullName(hdIDs[i]);
        if (diskName)
        {
            currentOptions.hds[i] = diskName;
        }
    }

    currentOptions.mouseInSlot4 = g_Slot4 == CT_MouseInterface;
    currentOptions.cpmInSlot5 = g_Slot5 == CT_Z80;
    currentOptions.hdInSlot7 = HD_CardIsEnabled();

    currentOptions.apple2Type = getApple2ComputerType();

    if (myGamepad)
    {
        currentOptions.joystick = myGamepad->name();
        currentOptions.joystickId = myGamepad->deviceId();
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

    QSettings settings; // the function will "modify" it
    myPreferences.setup(currentOptions, settings);

    if (myPreferences.exec())
    {
        const Preferences::Data newOptions = myPreferences.getData();

        if (currentOptions.apple2Type != newOptions.apple2Type)
        {
            const eApple2Type type = computerTypes[newOptions.apple2Type];
            SetApple2Type(type);
            REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), type);
            const eCpuType cpu = ProbeMainCpuDefault(type);
            SetMainCpu(cpu);
            REGSAVE(TEXT(REGVALUE_CPU_TYPE), cpu);
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
            myGamepad.reset();
            Paddle::instance() = std::make_shared<Paddle>();
        }
        else
        {
            if (newOptions.joystickId != currentOptions.joystickId)
            {
                myGamepad.reset(new QGamepad(newOptions.joystickId));
                Paddle::instance() = std::make_shared<GamepadPaddle>(myGamepad);
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

}

void QApple::on_actionSave_state_triggered()
{
    Snapshot_SaveState();
}

void QApple::on_actionLoad_state_triggered()
{
    emit endEmulator();
    Snapshot_LoadState();
    myEmulatorWindow->setWindowTitle(g_pAppTitle);
    myEmulator->updateVideo();
}

void QApple::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void QApple::on_actionAbout_triggered()
{
    QMessageBox::about(this, QApplication::applicationName(), "Apple ][ emulator\n\nBased on AppleWin\n");
}

QString getImageFilename()
{
    QString filenameTemplate = getScreenshotTemplate();
    static size_t counter = 0;

    const size_t maximum = 10000;
    while (counter < maximum)
    {
        const QString filename = filenameTemplate.arg(counter, 5, 10, QChar('0'));
        if (!QFile(filename).exists())
        {
            return filename;
        }
        ++counter;
    }

    return QString();
}

void QApple::on_actionScreenshot_triggered()
{
    const QString filename = getImageFilename();
    if (filename.isEmpty())
    {
        QMessageBox::warning(this, "Screenshot", "Cannot determine the screenshot filename.");
    }
    else
    {
        const bool ok = myEmulator->getScreen().save(filename);
        if (!ok)
        {
            const QString message = QString::fromUtf8("Cannot save screenshot to %1").arg(filename);
            QMessageBox::warning(this, "Screenshot", message);
        }
    }
}
