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

namespace
{

    void initialiseEmulator()
    {
        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, NULL);

        InitializeRegistry("/home/andrea/projects/cvs/A2E/applen.conf");

        LogFileOutput("Initialisation\n");

        ImageInitialize();
        DiskInitialize();
    }

    void startEmulator()
    {
        LoadConfiguration();

        CheckCpu();

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

void FrameDrawDiskLEDS(HDC x) {}
void FrameDrawDiskStatus(HDC x) {}
void FrameRefreshStatus(int x, bool) {}

// Keyboard

BYTE    KeybGetKeycode () {}
BYTE __stdcall KeybReadData (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}
BYTE __stdcall KeybReadFlag (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}

// Joystick

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}
BYTE __stdcall JoyReadPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}
BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft) {}

void VideoInitialize() {}

QApple::QApple(QWidget *parent) :
    QMainWindow(parent)
{
    setupUi(this);

    myVideo = new Video(mdiArea);
    mdiArea->addSubWindow(myVideo, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    myMSGap = 5;

    initialiseEmulator();
    startEmulator();
}

void QApple::timerEvent(QTimerEvent *event)
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
        myVideo->redrawScreen();
    }
}

void QApple::on_actionStart_triggered()
{
    myTimerID = startTimer(myMSGap);
    actionPause->setEnabled(true);
    actionStart->setEnabled(false);
}

void QApple::on_actionPause_triggered()
{
    killTimer(myTimerID);
    actionPause->setEnabled(false);
    actionStart->setEnabled(true);
}
