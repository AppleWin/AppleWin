#ifndef GAMEPADPADDLE_H
#define GAMEPADPADDLE_H

#include "linux/paddle.h"

class QGamepad;
class QString;

class GamepadPaddle : public Paddle
{
public:
    static std::shared_ptr<Paddle> fromName(const QString & name);
    GamepadPaddle(const std::shared_ptr<QGamepad> & gamepad);

    bool getButton(int i) const override;
    double getAxis(int i) const override;

private:
    const std::shared_ptr<QGamepad> myGamepad;
};

#endif // GAMEPADPADDLE_H
