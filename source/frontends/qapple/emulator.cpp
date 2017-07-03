#include "emulator.h"

Emulator::Emulator(QWidget *parent) :
    QFrame(parent)
{
    setupUi(this);
}

void Emulator::redrawScreen()
{
    video->update();
}
