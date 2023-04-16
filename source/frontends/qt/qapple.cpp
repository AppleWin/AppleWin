#include "qapple.h"
#include "ui_qapple.h"

#include "StdAfx.h"
#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "CPU.h"
#include "NTSC.h"
#include "SaveState.h"
#include "Speaker.h"

#include "linux/benchmark.h"
#include "linux/version.h"
#include "linux/context.h"

#include "emulator.h"
#include "memorycontainer.h"
#include "qdirectsound.h"
#include "gamepadpaddle.h"
#include "preferences.h"
#include "configuration.h"
#include "qtframe.h"

#include <QMdiSubWindow>
#include <QMessageBox>
#include <QFileDialog>
#include <QStyle>
#include <QSettings>
#include <QAudioOutput>
#include <QLabel>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>

#include <algorithm>

namespace
{

    void initialiseEmulator()
    {
        g_nAppMode = MODE_RUNNING;
        LogInit();
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

    qint64 emulatorTimeInMS()
    {
        const double timeInSeconds = g_nCumulativeCycles / g_fCurrentCLK6502;
        const qint64 timeInMS = timeInSeconds * 1000;
        return timeInMS;
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

    QIcon icon(QIcon::fromTheme(":/resources/APPLEWIN.ICO"));
    this->setWindowIcon(icon);

    ui->actionStart->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    ui->actionPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    ui->actionReboot->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));

    ui->actionSave_state->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    ui->actionLoad_state->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));

    mySaveStateLabel = new QLabel;
    statusBar()->addPermanentWidget(mySaveStateLabel);

    Registry::instance = std::make_shared<Configuration>();

    myPreferences = new Preferences(this);

    Emulator * emulator = new Emulator(ui->mdiArea);
    myEmulatorWindow = ui->mdiArea->addSubWindow(emulator, Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);

    myFrame = std::make_shared<QtFrame>(emulator, myEmulatorWindow);
    SetFrame(myFrame);

    readSettings();

    myOptions = GlobalOptions::fromQSettings();
    reloadOptions();

    on_actionPause_triggered();
    initialiseEmulator();
    myFrame->Begin();

    setAcceptDrops(true);
}

QApple::~QApple()
{
    delete ui;
}

void QApple::closeEvent(QCloseEvent * event)
{
    stopTimer();
    myFrame->End();
    QDirectSound::stop();

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

    CardManager & cardManager = GetCardMgr();

    int count = 0;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
    do
    {
        const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
        g_dwCyclesThisFrame += uActualCyclesExecuted;
        cardManager.Update(uActualCyclesExecuted);
        SpkrUpdate(uActualCyclesExecuted);

        // in case we run more than 1 frame
        g_dwCyclesThisFrame = g_dwCyclesThisFrame % dwClksPerFrame;
        ++count;
    }
    while (cardManager.GetDisk2CardMgr().IsConditionForFullSpeed() && (myElapsedTimer.elapsed() < target + myOptions.msFullSpeed));

    // just repaint each time, to make it simpler
    // we run @ 60 fps anyway
    myFrame->VideoPresentScreen();

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
    myFrame->SetZoom(1);
}

void QApple::on_actionX2_triggered()
{
    myFrame->SetZoom(2);
}

void QApple::on_action4_3_triggered()
{
    myFrame->Set43Ratio();
}

void QApple::on_actionReboot_triggered()
{
    PauseEmulator pause(this);

    emit endEmulator();
    mySaveStateLabel->clear();
    myFrame->Restart();
    myFrame->VideoPresentScreen();
}

void QApple::on_actionBenchmark_triggered()
{
    // call repaint as we really want to for a paintEvent() so we can time it properly
    // if video is based on OpenGLWidget, this is not enough though,
    // and benchmark results are bad.
    myFrame->SetForceRepaint(true);
    VideoBenchmark([this]() { myFrame->VideoRedrawScreen(); }, [this]() { myFrame->VideoPresentScreen(); });
    myFrame->SetForceRepaint(false);
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
        setAppleWinPreferences(myFrame, currentData, newData);
        myOptions.setData(newData.options);
        reloadOptions();
    }
}

void QApple::reloadOptions()
{
    myFrame->FrameRefreshStatus(DRAW_TITLE);

    Paddle::instance = GamepadPaddle::fromName(myOptions.gamepadName);
    Paddle::setSquaring(myOptions.gamepadSquaring);
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

    myFrame->LoadSnapshot();
    myFrame->FrameRefreshStatus(DRAW_TITLE);
    myFrame->VideoPresentScreen();

    QString message = QString("State file: %1").arg(file.filePath());
    mySaveStateLabel->setText(message);
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
        const bool ok = myFrame->saveScreen(filename);
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
    CardManager & cardManager = GetCardMgr();

    if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
    {
        dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6))->DriveSwap();
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
    myFrame->CycleVideoType();
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

void QApple::on_actionQuit_triggered()
{
    this->close();
}

void QApple::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void QApple::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();

    if (mimeData->hasUrls())
    {
        const QList<QUrl> urlList = mimeData->urls();

        if (!urlList.isEmpty())
        {
            // just use first file
            const QString file = urlList.first().toLocalFile();
            if (!file.isEmpty())
            {
                event->acceptProposedAction();
                loadStateFile(file);
            }
        }
   }
}
