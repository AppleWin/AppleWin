#include "video.h"

#include <QPainter>
#include <QKeyEvent>

#include "StdAfx.h"
#include "linux/videobuffer.h"
#include "linux/keyboard.h"
#include "linux/paddle.h"
#include "Common.h"
#include "CardManager.h"
#include "MouseInterface.h"
#include "AppleWin.h"

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
    QImage frameBuffer(data, width, height, QImage::Format_ARGB32_Premultiplied);

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
    QImage frameBuffer(data, width, height, QImage::Format_ARGB32_Premultiplied);

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
    QImage frameBuffer(data, width, height, QImage::Format_ARGB32_Premultiplied);

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

bool Video::event(QEvent *event)
{
    if (isEnabled() && (event->type() == QEvent::KeyPress))
    {
        // This code bypasses the special QWidget handling of QKeyEvents
        // Tab and Backtab (?) to move focus
        // KeyPad Navigation
        // F1 for whatsthis
        QKeyEvent *k = (QKeyEvent *)event;
        keyPressEvent(k);
        return true;
    }
    else
    {
        return VIDEO_BASECLASS::event(event);
    }
}

void Video::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat())
    {
        // Qt::Key_Alt does not seem to work
        // Qt::Key_AltGr & Qt::Key_Menu work well, but are on the same side
        const int key = event->key();
        switch (key)
        {
        case Qt::Key_AltGr:
            Paddle::setButtonReleased(Paddle::ourOpenApple);
            return;
        case Qt::Key_Menu:
            Paddle::setButtonReleased(Paddle::ourSolidApple);
            return;
        }
    }

    VIDEO_BASECLASS::keyReleaseEvent(event);
}

void Video::keyPressEvent(QKeyEvent *event)
{
    const int key = event->key();

    if (!event->isAutoRepeat())
    {
        switch (key)
        {
        case Qt::Key_AltGr:
            Paddle::setButtonPressed(Paddle::ourOpenApple);
            return;
        case Qt::Key_Menu:
            Paddle::setButtonPressed(Paddle::ourSolidApple);
            return;
        }
    }

    BYTE ch = 0;

    switch (key)
    {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        ch = 0x0d;
        break;
    case Qt::Key_Backspace: // same as AppleWin
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
    case Qt::Key_Tab:
        ch = 0x09;
        break;
    case 'A' ... 'Z':
    case ']':
    case '[':
    case '\\':
    {
        ch = key;
        const Qt::KeyboardModifiers modifiers = event->modifiers();
        if (modifiers & Qt::ControlModifier)
        {
            ch = (ch - 'A') + 1;
        }
        else if (modifiers & Qt::ShiftModifier)
        {
            ch += 'a' - 'A';
        }
        break;
    }
    case '-':
    {
        ch = key;
        const Qt::KeyboardModifiers modifiers = event->modifiers();
        if (modifiers & Qt::ControlModifier)
        {
           ch = 0x1f;
        }
        break;
    }
    default:
    {
        if (key < 0x80)
        {
            ch = key;
        }
    }
    }


    if (ch)
    {
        addKeyToBuffer(ch);
    }
    else
    {
        VIDEO_BASECLASS::keyPressEvent(event);
    }
}

void Video::mouseMoveEvent(QMouseEvent *event)
{
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
        int iX, iMinX, iMaxX;
        int iY, iMinY, iMaxY;
        cardManager.GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

        const QPointF p = event->localPos();
        const QSize s = size();

        const int newX = lround((p.x() / s.width()) * (iMaxX - iMinX) + iMinX);
        const int newY = lround((p.y() / s.height()) * (iMaxY - iMinY) + iMinY);

        const int dx = newX - iX;
        const int dy = newY - iY;

        int outOfBoundsX;
        int outOfBoundsY;
        cardManager.GetMouseCard()->SetPositionRel(dx, dy, &outOfBoundsX, &outOfBoundsY);

        event->accept();
    }
}

void Video::mousePressEvent(QMouseEvent *event)
{
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
        Qt::MouseButton button = event->button();
        switch (button)
        {
        case Qt::LeftButton:
            cardManager.GetMouseCard()->SetButton(BUTTON0, BUTTON_DOWN);
            break;
        case Qt::RightButton:
            cardManager.GetMouseCard()->SetButton(BUTTON1, BUTTON_DOWN);
            break;
        default:
            break;
        }
        event->accept();
    }
}

void Video::mouseReleaseEvent(QMouseEvent *event)
{
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
        Qt::MouseButton button = event->button();
        switch (button)
        {
        case Qt::LeftButton:
            cardManager.GetMouseCard()->SetButton(BUTTON0, BUTTON_UP);
            break;
        case Qt::RightButton:
            cardManager.GetMouseCard()->SetButton(BUTTON1, BUTTON_UP);
            break;
        default:
            break;
        }
        event->accept();
    }
}
