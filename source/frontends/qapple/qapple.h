#ifndef QAPPLE_H
#define QAPPLE_H


#include <QMainWindow>
#include <QElapsedTimer>
#include <QAudio>

#include <memory>

#include "options.h"


class QMdiSubWindow;
class Emulator;
class Preferences;

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

public slots:
    void startEmulator();

protected:
    virtual void closeEvent(QCloseEvent * event);
    virtual void timerEvent(QTimerEvent *event);

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

    void on_stateChanged(QAudio::State state);

    void on_actionLoad_state_from_triggered();

    void on_actionNext_video_mode_triggered();

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

    void stopTimer();
    void restartTimeCounters();
    void reloadOptions();

    int myTimerID;
    Preferences * myPreferences;

    QLabel * mySaveStateLabel;

    QElapsedTimer myElapsedTimer;
    QMdiSubWindow * myEmulatorWindow;
    Emulator * myEmulator;
    qint64 myCpuTimeReference;

    GlobalOptions myOptions;

private:
    Ui::QApple *ui;
};

#endif // QAPPLE_H
