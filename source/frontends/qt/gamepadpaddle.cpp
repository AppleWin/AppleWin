#include "gamepadpaddle.h"

#include <QGamepad>

std::shared_ptr<Paddle> GamepadPaddle::fromName(const QString & name)
{
    if (name.isEmpty())
    {
        return nullptr;
    }

    QGamepadManager * manager = QGamepadManager::instance();
    const QList<int> gamepads = manager->connectedGamepads();

    for (int id : gamepads)
    {
        if (name == manager->gamepadName(id))
        {
            const std::shared_ptr<QGamepad> gamepad = std::make_shared<QGamepad>(id);
            std::shared_ptr<Paddle> paddle = std::make_shared<GamepadPaddle>(gamepad);
            return paddle;
        }
    }

    return nullptr;
}

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

double GamepadPaddle::getAxis(int i) const
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

    return value;
}
