#ifndef VIDEO_H
#define VIDEO_H

#include <QOpenGLWidget>
#include <memory>

#include "graphics/graphicscache.h"

#define VIDEO_BASECLASS QOpenGLWidget
//#define VIDEO_BASECLASS QWidget


class Video : public VIDEO_BASECLASS
{
    Q_OBJECT
public:
    typedef GraphicsCache::ScreenPainter_t ScreenPainter_t;
    typedef GraphicsCache::Image_t Image_t;

    explicit Video(QWidget *parent = 0);

    Image_t getScreen() const;

signals:

public slots:

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:

    bool Update40ColCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool Update80ColCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateLoResCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDLoResCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateHiResCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDHiResCell(ScreenPainter_t & painter, int x, int y, int xpixel, int ypixel, int offset);

    // paint the whole screen
    // no scale applied
    void paint(ScreenPainter_t & painter);

    std::shared_ptr<const GraphicsCache> myGraphicsCache;
    Image_t myOffscreen;
};

#endif // VIDEO_H
