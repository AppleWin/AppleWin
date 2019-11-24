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
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Video.h"
#include "NTSC.h"
#include "SaveState.h"

#include "linux/data.h"
#include "linux/benchmark.h"
#include "linux/version.h"
#include "linux/paddle.h"

#include "emulator.h"
#include "memorycontainer.h"
#include "configuration.h"
#include "audiogenerator.h"
#include "gamepadpaddle.h"

#include <QMdiSubWindow>
#include <QMessageBox>
#include <QFileDialog>

namespace
{

    void initialiseEmulator()
    {
        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, nullptr);

        LogFileOutput("Initialisation\n");

        ImageInitialize();
        g_bFullSpeed = false;
    }

    /* In AppleWin there are 3 ways to reset the emulator
     *
     * 1) Hardware Change: it leaves the EnterMessageLoop() function
     * 2) ResetMachineState()
     * 3) CtrlReset()
     *
     * Here we implement something similar to 1)
     *
     */

    void startEmulator(QWidget * window, Emulator * emulator, const GlobalOptions & options)
    {
        LoadConfiguration();

        CheckCpu();

        SetWindowTitle();
        window->setWindowTitle(QString::fromStdString(g_pAppTitle));

        FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

        ResetDefaultMachineMemTypes();

        switch (options.slot0Card) {
        case 1: // Language Card
            SetExpansionMemType(CT_LanguageCard);
            break;
        case 2: // Saturn 64
            SetSaturnMemorySize(Saturn128K::kMaxSaturnBanks / 2);
            SetExpansionMemType(CT_Saturn128K);
            break;
        case 3: // Saturn 128
            SetSaturnMemorySize(Saturn128K::kMaxSaturnBanks);
            SetExpansionMemType(CT_Saturn128K);
            break;
        case 4: // RamWorks
            SetRamWorksMemorySize(options.ramWorksMemorySize);
            SetExpansionMemType(CT_RamWorksIII);
            break;
        }

        MemInitialize();
        VideoInitialize();

        emulator->displayLogo();

        sg_Disk2Card.Reset();
        HD_Reset();
    }

    void stopEmulator()
    {
        sg_Mouse.Uninitialize();
        sg_Mouse.Reset();
        MemDestroy();
    }

    void uninitialiseEmulator()
    {
        HD_Destroy();
        PrintDestroy();
        CpuDestroy();

        sg_Disk2Card.Destroy();
        ImageDestroy();
        fclose(g_fh);
        g_fh = nullptr;
    }

    qint64 emulatorTimeInMS()
    {
        const double timeInSeconds = g_nCumulativeCycles / g_fCurrentCLK6502;
        const qint64 timeInMS = timeInSeconds * 1000;
        return timeInMS;
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


QApple::PauseEmulator::PauseEmulator(QApple * qapple) : myQApple(qapple)
{
    myWasRunning = myQApple->actionPause->isEnabled();
    if (myWasRunning)
    {
        myQApple->actionPause->trigger();
    }
}

QApple::PauseEmulator::~PauseEmulator()
{
    if (myWasRunning)
    {
        myQApple->actionStart->trigger();
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

    connect(AudioGenerator::instance().getAudioOutput(), SIGNAL(stateChanged(QAudio::State)), this, SLOT(on_stateChanged(QAudio::State)));

    myOptions.reset(new GlobalOptions());
    reloadOptions();

    on_actionPause_triggered();
    initialiseEmulator();
    startEmulator(myEmulatorWindow, myEmulator, *myOptions);
}

void QApple::closeEvent(QCloseEvent *)
{
    stopTimer();
    stopEmulator();
    uninitialiseEmulator();
}

void QApple::on_stateChanged(QAudio::State state)
{
    AudioGenerator::instance().stateChanged(state);
}

void QApple::on_timer()
{
    AudioGenerator::instance().start();

    if (!myElapsedTimer.isValid())
    {
        myElapsedTimer.start();
        myCpuTimeReference = emulatorTimeInMS();
    }

    // target x ms ahead of where we are now, which is when the timer should be called again
    const qint64 target = myElapsedTimer.elapsed() + myOptions->msGap;
    const qint64 current = emulatorTimeInMS() - myCpuTimeReference;
    if (current > target)
    {
        // we got ahead of the timer by a lot

        // just check if we got something to write
        AudioGenerator::instance().writeAudio();

        // wait next call
        return;
    }

    const qint64 toRun = target - current;
    const double fUsecPerSec        = 1.e6;
    const qint64 nExecutionPeriodUsec = 1000 * toRun;

    const double fExecutionPeriodClks = g_fCurrentCLK6502 * (double(nExecutionPeriodUsec) / fUsecPerSec);
    const DWORD uCyclesToExecute = fExecutionPeriodClks;

    const bool bVideoUpdate = true;

    int count = 0;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
    do
    {
        const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
        g_dwCyclesThisFrame += uActualCyclesExecuted;
        sg_Disk2Card.UpdateDriveState(uActualCyclesExecuted);
        // in case we run more than 1 frame
        g_dwCyclesThisFrame = g_dwCyclesThisFrame % dwClksPerFrame;
        ++count;
    }
    while (sg_Disk2Card.IsConditionForFullSpeed() && (myElapsedTimer.elapsed() < target + myOptions->msFullSpeed));

    // just repaint each time, to make it simpler
    // we run @ 60 fps anyway
    myEmulator->updateVideo();

    if (count > 1)  // 1 is the non-full speed case
    {
        restartTimeCounters();
    }
    else
    {
        AudioGenerator::instance().writeAudio();
    }
}

void QApple::stopTimer()
{
    if (myTimerID)
    {
        restartTimeCounters();
        killTimer(myTimerID);
        myTimerID = 0;
    }
}

void QApple::restartTimeCounters()
{
    // let them restart next time
    AudioGenerator::instance().stop();
    myElapsedTimer.invalidate();
}

void QApple::on_actionStart_triggered()
{
    // always restart with the same timer gap that was last used
    myTimerID = startTimer(myOptions->msGap, Qt::PreciseTimer);
    actionPause->setEnabled(true);
    actionStart->setEnabled(false);
    restartTimeCounters();
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
    startEmulator(myEmulatorWindow, myEmulator, *myOptions);
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));
    myEmulator->updateVideo();
    restartTimeCounters();
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
    // this is to overcome an issue in GNOME where the open file dialog gets lost
    // if the emulator is running
    // at times it can be found in a separate desktop, or it magically reappears
    // but often it forces to terminate the emulator
    PauseEmulator pause(this);

    Preferences::Data currentData;
    getAppleWinPreferences(currentData);
    myOptions->getData(currentData);

    QSettings settings; // the function will "modify" it
    myPreferences.setup(currentData, settings);

    if (myPreferences.exec())
    {
        const Preferences::Data newData = myPreferences.getData();
        setAppleWinPreferences(currentData, newData);
        myOptions->setData(newData);
        reloadOptions();
    }

}

void QApple::reloadOptions()
{
    Paddle::instance() = GamepadPaddle::fromName(myOptions->gamepadName);
    AudioGenerator::instance().setOptions(myOptions->audioLatency, myOptions->silenceDelay, myOptions->volume);
}

void QApple::on_actionSave_state_triggered()
{
    Snapshot_SaveState();
}

void QApple::on_actionLoad_state_triggered()
{
    emit endEmulator();
    Snapshot_LoadState();
    SetWindowTitle();
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));
    myEmulator->updateVideo();
}

void QApple::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);
}

void QApple::on_actionAbout_triggered()
{
    QString qversion = QString::fromStdString(getVersion());
    QString message = QString("Apple ][ emulator\n\nBased on AppleWin %1\n").arg(qversion);
    QMessageBox::about(this, QApplication::applicationName(), message);
}

QString getImageFilename(const GlobalOptions & options)
{
    static size_t counter = 0;

    const size_t maximum = 10000;
    while (counter < maximum)
    {
        const QString filename = options.screenshotTemplate.arg(counter, 5, 10, QChar('0'));
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
    const QString filename = getImageFilename(*myOptions);
    if (filename.isEmpty())
    {
        QMessageBox::warning(this, "Screenshot", "Cannot determine the screenshot filename.");
    }
    else
    {
        const bool ok = myEmulator->saveScreen(filename);
        if (!ok)
        {
            const QString message = QString::fromUtf8("Cannot save screenshot to %1").arg(filename);
            QMessageBox::warning(this, "Screenshot", message);
        }
    }
}

void QApple::on_actionSwap_disks_triggered()
{
    PauseEmulator pause(this);

    // this might open a file dialog
    sg_Disk2Card.DriveSwap();
}

void QApple::on_actionLoad_state_from_triggered()
{
    PauseEmulator pause(this);

    QFileDialog stateFileDialog(this);
    stateFileDialog.setFileMode(QFileDialog::AnyFile);

    if (stateFileDialog.exec())
    {
        QStringList files = stateFileDialog.selectedFiles();
        if (files.size() == 1)
        {
            const QString & filename = files[0];
            const QFileInfo file(filename);
            const QString path = file.absoluteDir().canonicalPath();

            // this is useful as snapshots from the test
            // have relative disk location
            SetCurrentImageDir(path.toStdString().c_str());

            Snapshot_SetFilename(filename.toStdString().c_str());
            actionLoad_state->trigger();
        }
    }
}

void QApple::on_actionNext_video_mode_triggered()
{
    g_eVideoType++;
    if (g_eVideoType >= NUM_VIDEO_MODES)
        g_eVideoType = 0;

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();
}
