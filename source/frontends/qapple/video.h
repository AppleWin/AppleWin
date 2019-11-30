#ifndef VIDEO_H
#define VIDEO_H

#include <QOpenGLWidget>

#define VIDEO_BASECLASS QOpenGLWidget
//#define VIDEO_BASECLASS QWidget

class Video : public VIDEO_BASECLASS
{
    Q_OBJECT
public:
    explicit Video(QWidget *parent = nullptr);

    QImage getScreen() const;
    void displayLogo();

signals:

public slots:

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    QImage myLogo;
};

#endif // VIDEO_H
