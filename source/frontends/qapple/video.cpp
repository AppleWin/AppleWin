#include "video.h"

#include <QPainter>
#include <QKeyEvent>

#include "StdAfx.h"
#include "linux/data.h"
#include "MouseInterface.h"

namespace
{

    BYTE keyCode = 0;
    bool keyWaiting = false;

}

Video::Video(QWidget *parent) : VIDEO_BASECLASS(parent)
{
    setMouseTracking(true);

    myLogo = QImage(":/resources/APPLEWINLOGO.BMP").mirrored(false, true);
}

QImage Video::getScreen() const
{
    uint8_t * data;
    int width;
    int height;
    int sx, sy;
    int sw, sh;

    getScreenData(data, width, height, sx, sy, sw, sh);
    QImage frameBuffer(data, width, height, QImage::Format_RGB32);

    QImage screen = frameBuffer.copy(sx, sy, sw, sh);

    return screen;
}

void Video::displayLogo()
{
    uint8_t * data;
    int width;
    int height;
    int sx, sy;
    int sw, sh;

    getScreenData(data, width, height, sx, sy, sw, sh);
    QImage frameBuffer(data, width, height, QImage::Format_RGB32);

    QPainter painter(&frameBuffer);
    painter.drawImage(sx, sy, myLogo);
}

void Video::paintEvent(QPaintEvent *)
{
    uint8_t * data;
    int width;
    int height;
    int sx, sy;
    int sw, sh;

    getScreenData(data, width, height, sx, sy, sw, sh);
    QImage frameBuffer(data, width, height, QImage::Format_RGB32);

    const QSize actual = size();
    const double scaleX = double(actual.width()) / sw;
    const double scaleY = double(actual.height()) / sh;

    // then paint it on the widget with scale
    {
        QPainter painter(this);

        // scale and flip vertically
        const QTransform transform(scaleX, 0.0, 0.0, -scaleY, 0.0, actual.height());
        painter.setTransform(transform);

        painter.drawImage(0, 0, frameBuffer, sx, sy, sw, sh);
    }
}

void Video::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();

    BYTE ch = 0;

    switch (key)
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        ch = 0x0d;
        break;
    case Qt::Key_Left:
        ch = 0x08;
        break;
    case Qt::Key_Right:
        ch = 0x15;
        break;
    case Qt::Key_Up:
        ch = 0x0b;
        break;
    case Qt::Key_Down:
        ch = 0x0a;
        break;
    case Qt::Key_Delete:
        ch = 0x7f;
        break;
    case Qt::Key_Escape:
        ch = 0x1b;
        break;
    default:
        if (key < 0x80)
        {
            ch = key;
            if (ch >= 'A' && ch <= 'Z')
            {
                const Qt::KeyboardModifiers modifiers = event->modifiers();
                if (modifiers & Qt::ShiftModifier)
                {
                    ch += 'a' - 'A';
                }
            }
        }
        break;
    }

    if (ch)
    {
        keyCode = ch;
        keyWaiting = true;
        event->accept();
    }
}

void Video::mouseMoveEvent(QMouseEvent *event)
{
    if (sg_Mouse.IsActiveAndEnabled())
    {
        int iX, iMinX, iMaxX;
        int iY, iMinY, iMaxY;
        sg_Mouse.GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

        const QPointF p = event->localPos();
        const QSize s = size();

        const int newX = lround((p.x() / s.width()) * (iMaxX - iMinX) + iMinX);
        const int newY = lround((p.y() / s.height()) * (iMaxY - iMinY) + iMinY);

        const int dx = newX - iX;
        const int dy = newY - iY;

        int outOfBoundsX;
        int outOfBoundsY;
        sg_Mouse.SetPositionRel(dx, dy, &outOfBoundsX, &outOfBoundsY);

        event->accept();
    }
}

void Video::mousePressEvent(QMouseEvent *event)
{
    if (sg_Mouse.IsActiveAndEnabled())
    {
        Qt::MouseButton button = event->button();
        switch (button)
        {
        case Qt::LeftButton:
            sg_Mouse.SetButton(BUTTON0, BUTTON_DOWN);
            break;
        case Qt::RightButton:
            sg_Mouse.SetButton(BUTTON1, BUTTON_DOWN);
            break;
        default:
            break;
        }
        event->accept();
    }
}

void Video::mouseReleaseEvent(QMouseEvent *event)
{
    if (sg_Mouse.IsActiveAndEnabled())
    {
        Qt::MouseButton button = event->button();
        switch (button)
        {
        case Qt::LeftButton:
            sg_Mouse.SetButton(BUTTON0, BUTTON_UP);
            break;
        case Qt::RightButton:
            sg_Mouse.SetButton(BUTTON1, BUTTON_UP);
            break;
        default:
            break;
        }
        event->accept();
    }
}

// Keyboard

BYTE    KeybGetKeycode ()
{
  return keyCode;
}

BYTE KeybReadData()
{
    return keyCode | (keyWaiting ? 0x80 : 0x00);
}

BYTE KeybReadFlag()
{
    keyWaiting = false;
    return keyCode;
}
