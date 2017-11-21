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
#include "linux/benchmark.h"

#include "emulator.h"
#include "memorycontainer.h"
#include "configuration.h"

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
    myEmulator->updateVideo();
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
    const Preferences::Data currentOptions = getCurrentOptions(myGamepad);

    QSettings settings; // the function will "modify" it
    myPreferences.setup(currentOptions, settings);

    if (myPreferences.exec())
    {
        const Preferences::Data newOptions = myPreferences.getData();
        setNewOptions(currentOptions, newOptions, myGamepad);
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
