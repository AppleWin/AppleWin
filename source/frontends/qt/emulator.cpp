#include "emulator.h"
#include "ui_emulator.h"

#include <QMdiSubWindow>
#include "StdAfx.h"

#include <cmath>

Emulator::Emulator(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::Emulator)
{
    ui->setupUi(this);
}

Emulator::~Emulator()
{
    delete ui;
}

void Emulator::refreshScreen(const bool force)
{
    if (force)
    {
        ui->video->repaint();
    }
    else
    {
        ui->video->update();
    }
}

bool Emulator::saveScreen(const QString & filename) const
{
    return ui->video->getScreen().save(filename);
}

void Emulator::loadVideoSettings()
{
    ui->video->loadVideoSettings();
}

void Emulator::unloadVideoSettings()
{
    ui->video->unloadVideoSettings();
}

void Emulator::displayLogo()
{
    ui->video->displayLogo();
}

void Emulator::setVideoSize(QMdiSubWindow * window, const QSize & size)
{
    window->showNormal();

    // assume the extra space between widget and border is not affected by a resize
    const QSize gap = window->size() - ui->video->size();
    window->resize(size + gap);
}

void Emulator::setZoom(QMdiSubWindow * window, const int x)
{
    const QSize target = ui->video->minimumSize() * x;
    setVideoSize(window, target);
}

void Emulator::set43AspectRatio(QMdiSubWindow * window)
{
    // keep the same surface with 4:3 aspect ratio
    const QSize & size = ui->video->size();
    const double area = size.height() * size.width();

    const int numerator = 35;       // 7 * 40
    const int denominator = 24;     // 8 * 24

    const int x = sqrt(area / (numerator * denominator));
    const QSize target(numerator * x, denominator * x);
    setVideoSize(window, target);
}
