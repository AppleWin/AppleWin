#ifndef VIDEO_H
#define VIDEO_H

#include <QWidget>

#include <memory>

class Video : public QWidget
{
    Q_OBJECT
public:
    explicit Video(QWidget *parent = 0);

signals:

public slots:

protected:
    void paintEvent(QPaintEvent *event);

private:
    bool Update40ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool Update80ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);
    bool UpdateDHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset);

    std::shared_ptr<QPixmap> myCharset;
};

#endif // VIDEO_H
