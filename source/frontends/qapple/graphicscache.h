#ifndef GRAPHICSCACHE_H
#define GRAPHICSCACHE_H


#include <QPixmap>

class GraphicsCache
{
public:
    GraphicsCache();

    const QPixmap & text40Col() const
    {
        return myCharset40;
    }

    const QPixmap & text80Col() const
    {
        return myCharset80;
    }

    const QPixmap & hires() const
    {
        return myHiResMono;
    }

private:
    QPixmap myCharset40;
    QPixmap myCharset80;

    QPixmap myHiResMono;
};

#endif // GRAPHICSCACHE_H
