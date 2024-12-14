#include "qvideo.h"

#include <QPainter>
#include <QKeyEvent>

#include "StdAfx.h"
#include "linux/keyboardbuffer.h"
#include "linux/paddle.h"
#include "Common.h"
#include "CardManager.h"
#include "MouseInterface.h"
#include "Core.h"
#include "Video.h"
#include "Interface.h"

QVideo::QVideo(QWidget *parent) : QVIDEO_BASECLASS(parent)
{
    this->setMouseTracking(true);

    myLogo = QImage(":/resources/APPLEWINLOGO.BMP").mirrored(false, true);
}

void QVideo::loadVideoSettings()
{
    Video & video = GetVideo();

    mySX = video.GetFrameBufferBorderWidth();
    mySY = video.GetFrameBufferBorderHeight();
    mySW = video.GetFrameBufferBorderlessWidth();
    mySH = video.GetFrameBufferBorderlessHeight();
    myWidth = video.GetFrameBufferWidth();
    myHeight = video.GetFrameBufferHeight();

    myLogoX = mySX + video.GetFrameBufferCentringOffsetX();
    myLogoY = mySY + video.GetFrameBufferCentringOffsetY();

    myFrameBuffer = video.GetFrameBuffer();
}

void QVideo::unloadVideoSettings()
{
    myFrameBuffer = nullptr;
}

QImage QVideo::getScreenImage() const
{
    QImage frameBuffer(myFrameBuffer, myWidth, myHeight, QImage::Format_ARGB32_Premultiplied);
    return frameBuffer;
}

QImage QVideo::getScreen() const
{
    QImage frameBuffer = getScreenImage();
    QImage screen = frameBuffer.copy(mySX, mySY, mySW, mySH);

    return screen;
}

void QVideo::displayLogo()
{
    QImage frameBuffer = getScreenImage();

    QPainter painter(&frameBuffer);
    painter.drawImage(myLogoX, myLogoY, myLogo);
}

void QVideo::paintEvent(QPaintEvent *)
{
    QImage frameBuffer = getScreenImage();

    const QSize actual = size();
    const double scaleX = double(actual.width()) / mySW;
    const double scaleY = double(actual.height()) / mySH;

    // then paint it on the widget with scale
    {
        QPainter painter(this);

        // scale and flip vertically
        const QTransform transform(scaleX, 0.0, 0.0, -scaleY, 0.0, actual.height());
        painter.setTransform(transform);

        painter.drawImage(0, 0, frameBuffer, mySX, mySY, mySW, mySH);
    }
}

bool QVideo::event(QEvent *event)
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
        return QVIDEO_BASECLASS::event(event);
    }
}

void QVideo::keyReleaseEvent(QKeyEvent *event)
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

    QVIDEO_BASECLASS::keyReleaseEvent(event);
}

void QVideo::keyPressEvent(QKeyEvent *event)
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
        QVIDEO_BASECLASS::keyPressEvent(event);
    }
}

void QVideo::mouseMoveEvent(QMouseEvent *event)
{
    CardManager & cardManager = GetCardMgr();

    if (cardManager.IsMouseCardInstalled() && cardManager.GetMouseCard()->IsActiveAndEnabled())
    {
        int iX, iMinX, iMaxX;
        int iY, iMinY, iMaxY;
        cardManager.GetMouseCard()->GetXY(iX, iMinX, iMaxX, iY, iMinY, iMaxY);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        const QPointF p = event->localPos();
#else
        const QPointF p = event->position();
#endif
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

void QVideo::mousePressEvent(QMouseEvent *event)
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

void QVideo::mouseReleaseEvent(QMouseEvent *event)
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
