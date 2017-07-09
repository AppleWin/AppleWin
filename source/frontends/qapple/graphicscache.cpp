#include "graphicscache.h"

#include <QPainter>

namespace
{

    void halfScanLines(QPixmap & charset)
    {
        const int height = charset.height();
        const int width = charset.width();

        const QColor background = charset.toImage().pixelColor(0, height - 1);

        QPainter paint(&charset);
        paint.setPen(background);

        for (int i = 0; i < height; i += 2)
        {
            paint.drawLine(0, i, width - 1, i);
        }
    }

    QPixmap buildHiResMono()
    {
        const QColor black(Qt::black);
        const QColor white(Qt::white);

        QPixmap hires(14, 128 * 2);
        hires.fill(black);

        QPainter painter(&hires);

        for (int i = 0; i < 128; ++i)
        {
            for (int j = 0; j < 7; ++j)
            {
                if (i & (1 << j))
                {
                    painter.setPen(white);
                }
                else
                {
                    painter.setPen(black);
                }
                painter.drawLine(j * 2, i * 2, j * 2 + 2, i * 2);
            }
        }

        return hires;
    }

}

GraphicsCache::GraphicsCache()
{
    myCharset40.load(":/resources/CHARSET4.BMP");
    halfScanLines(myCharset40);

    myCharset80 = myCharset40.scaled(myCharset40.width() / 2, myCharset40.height());

    myHiResMono = buildHiResMono();
}
