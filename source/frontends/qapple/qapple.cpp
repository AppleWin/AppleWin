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

#include "emulator.h"

#include <QMdiSubWindow>

namespace
{

    void initialiseEmulator()
    {
        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, NULL);

        InitializeRegistry("../qapple/applen.conf");

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

    void uninitialiseEmulator()
    {
        HD_Destroy();
        PrintDestroy();
        CpuDestroy();
        MemDestroy();

        DiskDestroy();
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

// Joystick

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
    Q_UNUSED(pc)
    Q_UNUSED(addr)
    Q_UNUSED(bWrite)
    Q_UNUSED(d)
    Q_UNUSED(nCyclesLeft)
    return 0;
}

BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
    Q_UNUSED(pc)
    Q_UNUSED(addr)
    Q_UNUSED(bWrite)
    Q_UNUSED(d)
    Q_UNUSED(nCyclesLeft)
    return 0;
}

BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
    Q_UNUSED(pc)
    Q_UNUSED(addr)
    Q_UNUSED(bWrite)
    Q_UNUSED(d)
    Q_UNUSED(nCyclesLeft)
    return 0;
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
    QMainWindow(parent), myDiskFileDialog(this)
{
    setupUi(this);

    myDiskFileDialog.setFileMode(QFileDialog::AnyFile);

    myEmulator = new Emulator(mdiArea);
    myEmulatorWindow = mdiArea->addSubWindow(myEmulator, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    myMSGap = 5;
    myCurrentGap = myMSGap;

    initialiseEmulator();
    startEmulator();

    myEmulatorWindow->setWindowTitle(g_pAppTitle);
}

void QApple::closeEvent(QCloseEvent *)
{
    uninitialiseEmulator();
}

void QApple::timerEvent(QTimerEvent *)
{
    const double fUsecPerSec        = 1.e6;
    const UINT nExecutionPeriodUsec = 1000 * myMSGap;

    const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
    const DWORD uCyclesToExecute = fExecutionPeriodClks;

    const bool bVideoUpdate = false;
    const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
    g_dwCyclesThisFrame += uActualCyclesExecuted;

    if (g_dwCyclesThisFrame >= dwClksPerFrame)
    {
        g_dwCyclesThisFrame -= dwClksPerFrame;
        myEmulator->redrawScreen();
    }

    // 0 means run as soon as possible (i.e. no pending events)
    const int nextGap = DiskIsSpinning() ? 0 : myMSGap;
    setNextTimer(nextGap);
}

void QApple::setNextTimer(const int ms)
{
    if (ms != myCurrentGap)
    {
        killTimer(myTimerID);
        myCurrentGap = ms;
        myTimerID = startTimer(myCurrentGap);
    }
}

void QApple::on_actionStart_triggered()
{
    // always restart with the same timer gap that was last used
    myTimerID = startTimer(myCurrentGap);
    actionPause->setEnabled(true);
    actionStart->setEnabled(false);
}

void QApple::on_actionPause_triggered()
{
    killTimer(myTimerID);
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

void QApple::insertDisk(const int disk)
{
    if (myDiskFileDialog.exec())
    {
        QStringList files = myDiskFileDialog.selectedFiles();
        if (files.size() == 1)
        {
            const std::string filename = files[0].toStdString();
            const bool createMissingDisk = true;
            DiskInsert(disk, filename.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
        }
    }
}

void QApple::on_actionDisk_1_triggered()
{
    insertDisk(DRIVE_1);
}

void QApple::on_actionDisk_2_triggered()
{
    insertDisk(DRIVE_2);
}
