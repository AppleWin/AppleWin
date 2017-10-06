#include "gamepadpaddle.h"

GamepadPaddle::GamepadPaddle(const std::shared_ptr<QGamepad> & gamepad) : myGamepad(gamepad)
{

}

bool GamepadPaddle::getButton(int i) const
{
    switch (i)
    {
    case 0:
        return myGamepad->buttonA();
    case 1:
        return myGamepad->buttonB();
    default:
        return 0;
    }
}

int GamepadPaddle::getAxis(int i) const
{
    double value;
    switch (i)
    {
    case 0:
        value = myGamepad->axisLeftX();
        break;
    case 1:
        value = myGamepad->axisLeftY();
        break;
    default:
        value = 0.0;
    }

    const int pdl = int((value + 1.0) / 2.0 * 255.0);
    return pdl;
}
