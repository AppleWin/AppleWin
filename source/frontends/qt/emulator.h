#ifndef EMULATOR_H
#define EMULATOR_H

#include <QFrame>

class QMdiSubWindow;

namespace Ui {
class Emulator;
}

class Emulator : public QFrame
{
    Q_OBJECT

public:
    explicit Emulator(QWidget *parent = nullptr);
    ~Emulator();

    void updateVideo();
    void redrawScreen();    // regenerate image and repaint
    void refreshScreen();   // just repaint

    bool saveScreen(const QString & filename) const;
    void displayLogo();

    void setZoom(QMdiSubWindow * window, const int x);
    void set43AspectRatio(QMdiSubWindow * window);

private:
    void setVideoSize(QMdiSubWindow * window, const QSize & size);

private:
    Ui::Emulator *ui;
};

#endif // EMULATOR_H
