#ifndef QAPPLE_H
#define QAPPLE_H

#include "ui_qapple.h"

#include "video.h"

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

private:
    Video * myVideo;

    int myMSGap;
    int myTimerID;
};

#endif // QAPPLE_H
