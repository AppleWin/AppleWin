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

#include "linux/data.h"
#include "linux/configuration.h"
#include "linux/benchmark.h"
#include "linux/joy_input.h"

#include "emulator.h"
#include "memorycontainer.h"

#include <QMdiSubWindow>
#include <QMessageBox>

namespace
{

    void initialiseEmulator()
    {
        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, NULL);

        InitializeRegistry("../qapple/applen.conf");
        Input::initialise("/dev/input/by-id/usb-©Microsoft_Corporation_Controller_1BBE3DB-event-joystick");

        LogFileOutput("Initialisation\n");

        ImageInitialize();
        DiskInitialize();
    }

    void startEmulator()
    {
        LoadConfiguration();

        CheckCpu();

        SetWindowTitle();

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

QApple::QApple(QWidget *parent) :
    QMainWindow(parent), myTimerID(0), myPreferences(this)
{
    setupUi(this);

    myEmulator = new Emulator(mdiArea);
    myEmulatorWindow = mdiArea->addSubWindow(myEmulator, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
    myEmulatorWindow->setWindowTitle(g_pAppTitle);

    myMSGap = 5;

    initialiseEmulator();

    startEmulator();
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
            myEmulator->redrawScreen();
        }
        Input::instance().poll();
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
    startEmulator();
}

void QApple::on_actionBenchmark_triggered()
{
    VideoBenchmark([this]() { myEmulator->redrawScreen(); });
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

    myPreferences.setData(currentOptions);

    if (myPreferences.exec())
    {
        const Preferences::Data newOptions = myPreferences.getData();

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
    }

}
