#ifndef EMULATOR_H
#define EMULATOR_H

#include "ui_emulator.h"

class QMdiSubWindow;

class Emulator : public QFrame, private Ui::Emulator
{
    Q_OBJECT

public:
    explicit Emulator(QWidget *parent = nullptr);

    void updateVideo();
    void redrawScreen();    // regenerate image and repaint
    void refreshScreen();   // just repaint

    bool saveScreen(const QString & filename) const;
    void displayLogo();

    void setZoom(QMdiSubWindow * window, const int x);
    void set43AspectRatio(QMdiSubWindow * window);

private:
    void setVideoSize(QMdiSubWindow * window, const QSize & size);

};

#endif // EMULATOR_H
