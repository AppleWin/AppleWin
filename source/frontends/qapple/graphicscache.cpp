#include "graphicscache.h"

#include <QPainter>

namespace
{

    void halfScanLines(QPixmap & charset, const QColor & black)
    {
        const int height = charset.height();
        const int width = charset.width();

        QPainter paint(&charset);
        paint.setPen(black);

        for (int i = 0; i < height; i += 2)
        {
            paint.drawLine(0, i, width - 1, i);
        }
    }

    QPixmap buildHiResMono80()
    {
        const QColor black(Qt::black);
        const QColor white(Qt::white);

        QPixmap hires(7, 128 * 2); // 128 values 2 lines each

        QPainter painter(&hires);

        for (int i = 0; i < 128; ++i)
        {
            for (int j = 0; j < 7; ++j)
            {
                const QColor & color = (i & (1 << j)) ? white : black;
                painter.fillRect(j, i * 2, 1, 2, color);
            }
        }

        return hires;
    }

    enum LoResColor {
      BLACK,
      DEEP_RED,
      DARK_BLUE,
      MAGENTA,
      DARK_GREEN,
      DARK_GRAY,
      BLUE,
      LIGHT_BLUE,
      BROWN,
      ORANGE,
      LIGHT_GRAY,
      PINK,
      GREEN,
      YELLOW,
      AQUA,
      WHITE
    };

#define SETFRAMECOLOR(c, r, g, b) colors[c].setRgb(r, g, b);

    QPixmap buildLoResColor80()
    {
        std::vector<QColor> colors(16);

        SETFRAMECOLOR(BLACK,       0x00,0x00,0x00); // 0
        SETFRAMECOLOR(DEEP_RED,    0x9D,0x09,0x66); // 0xD0,0x00,0x30 -> Linards Tweaked 0x9D,0x09,0x66
        SETFRAMECOLOR(DARK_BLUE,   0x00,0x00,0x80); // 4 // not used
        SETFRAMECOLOR(MAGENTA,     0xC7,0x34,0xFF); // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF

        SETFRAMECOLOR(DARK_GREEN,  0x00,0x80,0x00); // 2 // not used
        SETFRAMECOLOR(DARK_GRAY,   0x80,0x80,0x80); // F8
        SETFRAMECOLOR(BLUE,        0x0D,0xA1,0xFF); // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
        SETFRAMECOLOR(LIGHT_BLUE,  0xAA,0xAA,0xFF); // 0x60,0xA0,0xFF -> Linards Tweaked 0xAA,0xAA,0xFF

        SETFRAMECOLOR(BROWN,       0x55,0x55,0x00); // 0x80,0x50,0x00 -> Linards Tweaked 0x55,0x55,0x00
        SETFRAMECOLOR(ORANGE,      0xF2,0x5E,0x00); // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
        SETFRAMECOLOR(LIGHT_GRAY,  0xC0,0xC0,0xC0); // 7 // GR: COLOR=10
        SETFRAMECOLOR(PINK,        0xFF,0x89,0xE5); // 0xFF,0x90,0x80 -> Linards Tweaked 0xFF,0x89,0xE5

        SETFRAMECOLOR(GREEN,       0x38,0xCB,0x00); // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
        SETFRAMECOLOR(YELLOW,      0xD5,0xD5,0x1A); // FB Linards Tweaked 0xFF,0xFF,0x00 -> 0xD5,0xD5,0x1A
        SETFRAMECOLOR(AQUA,        0x62,0xF6,0x99); // 0x40,0xFF,0x90 -> Linards Tweaked 0x62,0xF6,0x99
        SETFRAMECOLOR(WHITE,       0xFF,0xFF,0xFF); // FF


        QPixmap lores(7, 256 * 16);  // 256 values 16 lines each

        QPainter painter(&lores);

        for (int i = 0; i < 256; ++i)
        {
            const int top = i & 0b1111;
            const int bottom = (i >> 4) & 0b1111;

            painter.fillRect(0, i * 16,     7, 8, colors[top]);
            painter.fillRect(0, i * 16 + 8, 7, 8, colors[bottom]);
        }

        return lores;
    }

}


GraphicsCache::GraphicsCache()
{
    const QColor black(Qt::black);

    myCharset40.load(":/resources/CHARSET4.BMP");
    halfScanLines(myCharset40, black);

    myCharset80 = myCharset40.scaled(myCharset40.width() / 2, myCharset40.height());
    // it was already half scan

    myHiResMono80 = buildHiResMono80();
    halfScanLines(myHiResMono80, black);

    myHiResMono40 = myHiResMono80.scaled(myHiResMono80.width() * 2, myHiResMono80.height());
    // it was already half scan

    myLoResColor80 = buildLoResColor80();
    halfScanLines(myLoResColor80, black);

    myLoResColor40 = myLoResColor80.scaled(myLoResColor80.width() * 2, myLoResColor80.height());
    // it was already half scan
}
