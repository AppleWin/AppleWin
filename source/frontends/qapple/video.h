#ifndef VIDEO_H
#define VIDEO_H

#include <QOpenGLWidget>
#include <memory>

#define VIDEO_BASECLASS QOpenGLWidget
//#define VIDEO_BASECLASS QWidget

class GraphicsCache;

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
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    bool Update40ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool Update80ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);

    // paint the whole screen
    // no scale applied
    void paint(QPainter & painter);

    std::shared_ptr<const GraphicsCache> myGraphicsCache;
};

#endif // VIDEO_H
