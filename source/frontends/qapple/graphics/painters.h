#ifndef PAINTERS_H
#define PAINTERS_H

#include <QImage>
#include <QPixmap>
#include <QPainter>

inline void drawAny(QPainter & painter, const QPixmap & image)
{
    painter.drawPixmap(0, 0, image);
}

inline void drawAny(QPainter & painter, const QImage & image)
{
    painter.drawImage(0, 0, image);
}

inline QPixmap toPixmap(const QPixmap & image)
{
    return image;
}

inline QPixmap toPixmap(const QImage & image)
{
    return QPixmap::fromImage(image);
}


class FastPainter
{
public:
    typedef QImage Source_t;

    FastPainter(QImage * image)
        : myImage(image)
    {
        myBits = myImage->bits();
        myBytesPerLine = myImage->bytesPerLine();

        const QPixelFormat format = myImage->pixelFormat();
        myBytesPerPixel = format.bitsPerPixel() / 8;
    }

    void draw(int x, int y, const QImage & source, int sx, int sy, int wx, int wy)
    {
        // this only works if the 2 images have the same format
        const int destOffset = x * myBytesPerPixel;
        const int sourceOffset = sx * myBytesPerPixel;
        const int length = wx * myBytesPerPixel;

        const int sourceBytesPerLine = source.bytesPerLine();
        const uchar * sourceBits = source.bits();

        for (int iy = 0; iy < wy; iy += 1)
        {
            const uchar * sourceLine = sourceBits + sourceBytesPerLine * (sy + iy);
            uchar * destLine = myBits + myBytesPerLine * (y + iy);
            memcpy(destLine + destOffset, sourceLine + sourceOffset, length);
        }
    }

    static QImage create(int width, int height)
    {
        return QImage(width, height, theFormat);
    }

    static void convert(QImage & image)
    {
        image = image.convertToFormat(theFormat);
    }

private:
    static const QImage::Format theFormat = QImage::Format_RGB32;

    QImage * myImage;

    uchar * myBits;
    int myBytesPerPixel;
    int myBytesPerLine;
};

class Painter : QPainter
{
public:
    typedef QPixmap Source_t;
    //typedef QImage Source_t;

    Painter(QPaintDevice * image)
        : QPainter(image)
    {
    }

    void draw(int x, int y, const QImage & source, int sx, int sy, int wx, int wy)
    {
        drawImage(x, y, source, sx, sy, wx, wy);
    }

    void draw(int x, int y, const QPixmap & source, int sx, int sy, int wx, int wy)
    {
        drawPixmap(x, y, source, sx, sy, wx, wy);
    }
    
    static QPixmap create(int width, int height)
    {
        return QPixmap(width, height);
    }

    static void convert(Source_t & image)
    {
        Q_UNUSED(image)
    }

};

#endif // PAINTERS_H
