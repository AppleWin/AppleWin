#ifndef EMULATOR_H
#define EMULATOR_H

#include "ui_emulator.h"

class QMdiSubWindow;

class Emulator : public QFrame, private Ui::Emulator
{
    Q_OBJECT

public:
    explicit Emulator(QWidget *parent = 0);

    void updateVideo();
    void repaintVideo();

    const QPixmap & getScreen() const;

    void setZoom(QMdiSubWindow * window, const int x);
    void set43AspectRatio(QMdiSubWindow * window);

private:
    void setVideoSize(QMdiSubWindow * window, const QSize & size);

};

#endif // EMULATOR_H
