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

    const QPixmap & hires40() const
    {
        return myHiResMono40;
    }

    const QPixmap & hires80() const
    {
        return myHiResMono80;
    }

    const QPixmap & lores40() const
    {
        return myLoResColor40;
    }

    const QPixmap & lores80() const
    {
        return myLoResColor80;
    }

private:
    QPixmap myCharset40;
    QPixmap myCharset80;

    QPixmap myHiResMono40;
    QPixmap myHiResMono80;

    QPixmap myLoResColor40;
    QPixmap myLoResColor80;
};

#endif // GRAPHICSCACHE_H
