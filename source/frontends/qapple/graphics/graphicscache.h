#ifndef GRAPHICSCACHE_H
#define GRAPHICSCACHE_H

#include <QPixmap>
#include <QImage>

#include "graphics/painters.h"

class GraphicsCache
{
public:
    GraphicsCache();

    typedef FastPainter ScreenPainter_t;
    typedef ScreenPainter_t::Source_t Image_t;

    static Image_t createBlackScreenImage();

    const Image_t & text40Col() const
    {
        return myCharset40;
    }

    const Image_t & text80Col() const
    {
        return myCharset80;
    }

    const Image_t & hires40() const
    {
        return myHiResMono40;
    }

    const Image_t & hires80() const
    {
        return myHiResMono80;
    }

    const Image_t & lores40() const
    {
        return myLoResColor40;
    }

    const Image_t & lores80() const
    {
        return myLoResColor80;
    }

private:
    Image_t myCharset40;
    Image_t myCharset80;

    Image_t myHiResMono40;
    Image_t myHiResMono80;

    Image_t myLoResColor40;
    Image_t myLoResColor80;
};

#endif // GRAPHICSCACHE_H
