#ifndef QAPPLE_H
#define QAPPLE_H

#include "ui_qapple.h"

#include <QElapsedTimer>
#include <QAudio>

#include <memory>
#include "preferences.h"

class Emulator;
class GlobalOptions;

class QApple : public QMainWindow, private Ui::QApple
{
    Q_OBJECT

public:
    explicit QApple(QWidget *parent = nullptr);

signals:
    void endEmulator();

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
    Preferences myPreferences;

    QElapsedTimer myElapsedTimer;
    QMdiSubWindow * myEmulatorWindow;
    Emulator * myEmulator;
    qint64 myCpuTimeReference;

    std::shared_ptr<GlobalOptions> myOptions;
};

#endif // QAPPLE_H
