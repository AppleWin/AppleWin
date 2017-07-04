#ifndef VIDEO_H
#define VIDEO_H

#include <QOpenGLWidget>

//#define VIDEO_BASECLASS QOpenGLWidget
#define VIDEO_BASECLASS QWidget

class Video : public VIDEO_BASECLASS
{
    Q_OBJECT
public:
    explicit Video(QWidget *parent = 0);

signals:

public slots:

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);

private:
    bool Update40ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool Update80ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);

    QPixmap myCharset40;
    QPixmap myCharset80;
};

#endif // VIDEO_H
