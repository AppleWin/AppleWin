#ifndef QAPPLE_H
#define QAPPLE_H

#include "ui_qapple.h"

class Emulator;

class QApple : public QMainWindow, private Ui::QApple
{
    Q_OBJECT

public:
    explicit QApple(QWidget *parent = 0);

protected:
    virtual void timerEvent(QTimerEvent *event);

private slots:
    void on_actionStart_triggered();

    void on_actionPause_triggered();

    void on_actionX1_triggered();

    void on_actionX2_triggered();

    void on_action4_3_triggered();

private:
    QMdiSubWindow * myEmulatorWindow;
    Emulator * myEmulator;

    int myMSGap;
    int myTimerID;
};

#endif // QAPPLE_H
