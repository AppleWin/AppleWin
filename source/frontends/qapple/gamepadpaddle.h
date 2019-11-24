#ifndef GAMEPADPADDLE_H
#define GAMEPADPADDLE_H

#include <QGamepad>

#include "linux/paddle.h"

class GamepadPaddle : public Paddle
{
public:
    static std::shared_ptr<Paddle> fromName(const QString & name);

    virtual bool getButton(int i) const;
    virtual int getAxis(int i) const;

private:
    GamepadPaddle(const std::shared_ptr<QGamepad> & gamepad);
    const std::shared_ptr<QGamepad> myGamepad;
};

#endif // GAMEPADPADDLE_H
