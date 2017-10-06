#ifndef GAMEPADPADDLE_H
#define GAMEPADPADDLE_H

#include <QGamepad>

#include "linux/paddle.h"

class GamepadPaddle : public Paddle
{
public:
    GamepadPaddle(const std::shared_ptr<QGamepad> & gamepad);

    virtual bool getButton(int i) const;
    virtual int getAxis(int i) const;

private:
    const std::shared_ptr<QGamepad> myGamepad;
};

#endif // GAMEPADPADDLE_H
