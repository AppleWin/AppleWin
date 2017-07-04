#ifndef EMULATOR_H
#define EMULATOR_H

#include "ui_emulator.h"

class QMdiSubWindow;

class Emulator : public QFrame, private Ui::Emulator
{
    Q_OBJECT

public:
    explicit Emulator(QWidget *parent = 0);

    void redrawScreen();

    void setZoom(QMdiSubWindow * window, int x);
    void set43AspectRatio(QMdiSubWindow * window);

private:
    void setVideoSize(QMdiSubWindow * window, const QSize & size);

};

#endif // EMULATOR_H
