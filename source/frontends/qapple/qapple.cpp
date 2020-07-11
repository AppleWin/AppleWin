#include "qapple.h"
#include "ui_qapple.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Video.h"
#include "SoundCore.h"
#include "NTSC.h"
#include "SaveState.h"
#include "Speaker.h"
#include "Riff.h"

#include "linux/data.h"
#include "linux/benchmark.h"
#include "linux/version.h"
#include "linux/paddle.h"

#include "emulator.h"
#include "memorycontainer.h"
#include "qdirectsound.h"
#include "gamepadpaddle.h"
#include "preferences.h"

#include <QMdiSubWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QStyle>
#include <QSettings>
#include <QAudioOutput>
#include <QLabel>

#include <algorithm>

namespace
{

    void initialiseEmulator()
    {
#ifdef RIFF_MB
        RiffInitWriteFile("/tmp/Mockingboard.wav", 44100, 2);
#endif

        g_fh = fopen("/tmp/applewin.txt", "w");
        setbuf(g_fh, nullptr);

        LogFileOutput("Initialisation\n");

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

    void loadEmulator(QWidget * window, Emulator * emulator, const GlobalOptions & options)
    {
        ImageInitialize();
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

        DSInit();
        MB_Initialize();
        SpkrInitialize();
        MemInitialize();
        VideoInitialize();

        emulator->displayLogo();

        g_CardMgr.GetDisk2CardMgr().Reset();
        HD_Reset();
    }

    void unloadEmulator()
    {
        CMouseInterface* pMouseCard = g_CardMgr.GetMouseCard();
        if (pMouseCard)
        {
            pMouseCard->Reset();
        }
        HD_Destroy();
        PrintDestroy();
        MemDestroy();
        SpkrDestroy();
        VideoDestroy();
        MB_Destroy();
        DSUninit();
        CpuDestroy();

        g_CardMgr.GetDisk2CardMgr().Destroy();
        ImageDestroy();
        LogDone();
        RiffFinishWriteFile();

        QDirectSound::stop();
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
    myWasRunning = myQApple->ui->actionPause->isEnabled();
    if (myWasRunning)
    {
        myQApple->ui->actionPause->trigger();
    }
}

QApple::PauseEmulator::~PauseEmulator()
{
    if (myWasRunning)
    {
        myQApple->ui->actionStart->trigger();
    }
}

QApple::QApple(QWidget *parent) :
    QMainWindow(parent),
    myTimerID(0),
    ui(new Ui::QApple)
{
    ui->setupUi(this);

    ui->actionStart->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->actionPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->actionReboot->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));

    ui->actionSave_state->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->actionLoad_state->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));

    mySaveStateLabel = new QLabel;
    statusBar()->addPermanentWidget(mySaveStateLabel);

    myPreferences = new Preferences(this);

    myEmulator = new Emulator(ui->mdiArea);
    myEmulatorWindow = ui->mdiArea->addSubWindow(myEmulator, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    readSettings();

    myOptions = GlobalOptions::fromQSettings();
    reloadOptions();

    on_actionPause_triggered();
    initialiseEmulator();
    loadEmulator(myEmulatorWindow, myEmulator, myOptions);
}

QApple::~QApple()
{
    delete ui;
}

void QApple::closeEvent(QCloseEvent * event)
{
    stopTimer();
    unloadEmulator();

    QSettings settings;
    settings.setValue("QApple/window/geometry", saveGeometry().toBase64());
    settings.setValue("QApple/window/windowState", saveState().toBase64());
    settings.setValue("QApple/emulator/geometry", myEmulatorWindow->saveGeometry().toBase64());

    QMainWindow::closeEvent(event);
}

void QApple::readSettings()
{
    // this does not work completely in wayland
    // position is not restored
    QSettings settings;
    const QByteArray windowGeometry = QByteArray::fromBase64(settings.value("QApple/window/geometry").toByteArray());
    const QByteArray windowState = QByteArray::fromBase64(settings.value("QApple/window/state").toByteArray());
    const QByteArray emulatorGeometry = QByteArray::fromBase64(settings.value("QApple/emulator/geometry").toByteArray());

    restoreGeometry(windowGeometry);
    restoreState(windowState);
    myEmulatorWindow->restoreGeometry(emulatorGeometry);
}

void QApple::startEmulator()
{
    ui->actionStart->trigger();
}

void QApple::on_timer()
{
    QDirectSound::start();

    if (!myElapsedTimer.isValid())
    {
        myElapsedTimer.start();
        myCpuTimeReference = emulatorTimeInMS();
    }

    // target x ms ahead of where we are now, which is when the timer should be called again
    const qint64 target = myElapsedTimer.elapsed() + myOptions.msGap;
    const qint64 current = emulatorTimeInMS() - myCpuTimeReference;
    if (current > target)
    {
        // we got ahead of the timer by a lot

        // just check if we got something to write
        QDirectSound::writeAudio();

        // wait next call
        return;
    }

    const qint64 maximumToRum = 10 * myOptions.msGap;  // just to avoid crazy times (e.g. debugging)
    const qint64 toRun = std::min(target - current, maximumToRum);
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
        g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);
        MB_PeriodicUpdate(uActualCyclesExecuted);
        SpkrUpdate(uActualCyclesExecuted);

        // in case we run more than 1 frame
        g_dwCyclesThisFrame = g_dwCyclesThisFrame % dwClksPerFrame;
        ++count;
    }
    while (g_CardMgr.GetDisk2CardMgr().IsConditionForFullSpeed() && (myElapsedTimer.elapsed() < target + myOptions.msFullSpeed));

    // just repaint each time, to make it simpler
    // we run @ 60 fps anyway
    myEmulator->updateVideo();

    if (count > 1)  // 1 is the non-full speed case
    {
        restartTimeCounters();
    }
    else
    {
        QDirectSound::writeAudio();
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
    QDirectSound::stop();
    myElapsedTimer.invalidate();
}

void QApple::on_actionStart_triggered()
{
    // always restart with the same timer gap that was last used
    myTimerID = startTimer(myOptions.msGap, Qt::PreciseTimer);
    ui->actionPause->setEnabled(true);
    ui->actionStart->setEnabled(false);
    restartTimeCounters();
}

void QApple::on_actionPause_triggered()
{
    stopTimer();
    ui->actionPause->setEnabled(false);
    ui->actionStart->setEnabled(true);
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
    PauseEmulator pause(this);

    emit endEmulator();
    mySaveStateLabel->clear();
    unloadEmulator();
    loadEmulator(myEmulatorWindow, myEmulator, myOptions);
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));
    myEmulator->updateVideo();
}

void QApple::on_actionBenchmark_triggered()
{
    // call repaint as we really want to for a paintEvent() so we can time it properly
    // if video is based on OpenGLWidget, this is not enough though,
    // and benchmark results are bad.
    VideoBenchmark([this]() { myEmulator->redrawScreen(); }, [this]() { myEmulator->refreshScreen(); });
    on_actionReboot_triggered();
}

void QApple::timerEvent(QTimerEvent *)
{
    on_timer();
}

void QApple::on_actionMemory_triggered()
{
    MemoryContainer * container = new MemoryContainer(ui->mdiArea);
    QMdiSubWindow * window = ui->mdiArea->addSubWindow(container);

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

    PreferenceData currentData;
    getAppleWinPreferences(currentData);
    currentData.options = myOptions;

    QSettings settings; // the function will "modify" it
    myPreferences->setup(currentData, settings);

    if (myPreferences->exec())
    {
        const PreferenceData newData = myPreferences->getData();
        setAppleWinPreferences(currentData, newData);
        myOptions.setData(newData.options);
        reloadOptions();
    }

}

void QApple::reloadOptions()
{
    SetWindowTitle();
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));

    Paddle::instance() = GamepadPaddle::fromName(myOptions.gamepadName);
    QDirectSound::setOptions(myOptions.audioLatency);
}

void QApple::on_actionSave_state_triggered()
{
    Snapshot_SaveState();
}

void QApple::on_actionLoad_state_triggered()
{
    PauseEmulator pause(this);

    emit endEmulator();

    const std::string & filename = Snapshot_GetFilename();

    const QFileInfo file(QString::fromStdString(filename));
    const QString path = file.absolutePath();
    // this is useful as snapshots from the test
    // have relative disk location
    SetCurrentImageDir(path.toStdString().c_str());

    Snapshot_LoadState();
    SetWindowTitle();
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));
    QString message = QString("State file: %1").arg(file.filePath());
    mySaveStateLabel->setText(message);
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
    const QString filename = getImageFilename(myOptions);
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

    if (g_CardMgr.QuerySlot(SLOT6) == CT_Disk2)
    {
        dynamic_cast<Disk2InterfaceCard*>(g_CardMgr.GetObj(SLOT6))->DriveSwap();
    }
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
            loadStateFile(filename);
        }
    }
}

void QApple::on_actionNext_video_mode_triggered()
{
    g_eVideoType++;
    if (g_eVideoType >= NUM_VIDEO_MODES)
        g_eVideoType = 0;

    SetWindowTitle();
    myEmulatorWindow->setWindowTitle(QString::fromStdString(g_pAppTitle));

    Config_Save_Video();
    VideoReinitialize();
    VideoRedrawScreen();
}

void QApple::loadStateFile(const QString & filename)
{
    const QFileInfo path(filename);
    // store it as absolute path
    // use case is:
    // later, when we change dir to allow loading of disks relative to the yamls file,
    // a snapshot relative path would be lost
    Snapshot_SetFilename(path.absoluteFilePath().toStdString().c_str());
    ui->actionLoad_state->trigger();
}
