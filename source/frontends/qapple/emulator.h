#ifndef EMULATOR_H
#define EMULATOR_H

#include "ui_emulator.h"

class Emulator : public QFrame, private Ui::Emulator
{
    Q_OBJECT

public:
    explicit Emulator(QWidget *parent = 0);

    void redrawScreen();
};

#endif // EMULATOR_H
