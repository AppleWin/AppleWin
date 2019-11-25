#include "emulator.h"

#include <QMdiSubWindow>
#include "StdAfx.h"
#include "Common.h"
#include "Video.h"
#include "NTSC.h"

#include <cmath>

Emulator::Emulator(QWidget *parent) :
    QFrame(parent)
{
    setupUi(this);
}

void Emulator::updateVideo()
{
    video->update();
}

void Emulator::redrawScreen()
{
    NTSC_SetVideoMode( g_uVideoMode );
    NTSC_VideoRedrawWholeScreen();
    refreshScreen();
}

void Emulator::refreshScreen()
{
    video->repaint();
}

bool Emulator::saveScreen(const QString & filename) const
{
    return video->getScreen().save(filename);
}

void Emulator::displayLogo()
{
    video->displayLogo();
}

void Emulator::setVideoSize(QMdiSubWindow * window, const QSize & size)
{
    window->showNormal();

    // assume the extra space between widget and border is not affected by a resize
    const QSize gap = window->size() - video->size();
    window->resize(size + gap);
}

void Emulator::setZoom(QMdiSubWindow * window, const int x)
{
    const QSize target = video->minimumSize() * x;
    setVideoSize(window, target);
}

void Emulator::set43AspectRatio(QMdiSubWindow * window)
{
    // keep the same surface with 4:3 aspect ratio
    const QSize & size = video->size();
    const double area = size.height() * size.width();

    const int numerator = 35;       // 7 * 40
    const int denominator = 24;     // 8 * 24

    const int x = sqrt(area / (numerator * denominator));
    const QSize target(numerator * x, denominator * x);
    setVideoSize(window, target);
}
