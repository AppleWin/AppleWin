#ifndef QVIDEO_H
#define QVIDEO_H

#include <QOpenGLWidget>

#define QVIDEO_BASECLASS QOpenGLWidget
// #define QVIDEO_BASECLASS QWidget

class QVideo : public QVIDEO_BASECLASS
{
    Q_OBJECT
public:
    explicit QVideo(QWidget *parent = nullptr);

    QImage getScreen() const;
    void loadVideoSettings();
    void unloadVideoSettings();
    void displayLogo();

signals:

public slots:

protected:
    bool event(QEvent *event) override;

    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QImage myLogo;

    int mySX;
    int mySY;
    int mySW;
    int mySH;
    int myWidth;
    int myHeight;

    int myLogoX;
    int myLogoY;

    quint8 *myFrameBuffer;

    QImage getScreenImage() const;
};

#endif // QVIDEO_H
