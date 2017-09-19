#ifndef QAPPLE_H
#define QAPPLE_H

#include "ui_qapple.h"

#include <QElapsedTimer>

class Emulator;

class QApple : public QMainWindow, private Ui::QApple
{
    Q_OBJECT

public:
    explicit QApple(QWidget *parent = 0);

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

private:

    void stopTimer();

    QElapsedTimer myElapsedTimer;
    QMdiSubWindow * myEmulatorWindow;
    Emulator * myEmulator;

    int myMSGap;

    int myTimerID;
};

#endif // QAPPLE_H
