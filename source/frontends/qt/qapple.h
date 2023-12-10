#ifndef QAPPLE_H
#define QAPPLE_H


#include <QMainWindow>
#include <QElapsedTimer>
#include <QAudio>

#include <memory>

#include "options.h"


class QMdiSubWindow;
class Preferences;
class QtFrame;

namespace Ui {
class QApple;
}

class QLabel;

class QApple : public QMainWindow
{
    Q_OBJECT

public:
    explicit QApple(QWidget *parent = nullptr);
    ~QApple();

    void loadStateFile(const QString & stateFile);

signals:
    void endEmulator();
    void endFrame(const qint64 speed, const qint64 target);

public slots:
    void startEmulator();

protected:
    void closeEvent(QCloseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:

    void on_actionStart_triggered();

    void on_actionPause_triggered();

    void on_actionX1_triggered();

    void on_actionX2_triggered();

    void on_action4_3_triggered();

    void on_actionReboot_triggered();

    void on_actionBenchmark_triggered();

    void on_timer();

    void on_actionMemory_triggered();

    void on_actionOptions_triggered();

    void on_actionSave_state_triggered();

    void on_actionLoad_state_triggered();

    void on_actionAbout_Qt_triggered();

    void on_actionAbout_triggered();

    void on_actionScreenshot_triggered();

    void on_actionSwap_disks_triggered();

    void on_actionLoad_state_from_triggered();

    void on_actionNext_video_mode_triggered();

    void on_actionQuit_triggered();

    void on_actionAudio_Info_triggered();

private:

    // helper class to pause the emulator and restart at the end of the block
    class PauseEmulator
    {
    public:
        PauseEmulator(QApple * qapple);

        ~PauseEmulator();

    private:
        bool myWasRunning;
        QApple * myQApple;
    };

    void readSettings();
    void stopTimer();
    void restartTimeCounters();
    void reloadOptions();

    int myTimerID;
    Preferences * myPreferences;

    QLabel * mySaveStateLabel;

    std::shared_ptr<QtFrame> myFrame;
    QElapsedTimer myElapsedTimer;
    qint64 myStartCycles;
    qint64 myOrgStartCycles;
    QMdiSubWindow * myEmulatorWindow;

    GlobalOptions myOptions;

private:
    Ui::QApple *ui;
};

#endif // QAPPLE_H
