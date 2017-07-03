#include "video.h"

#include <QPainter>

#include "StdAfx.h"
#include "Video.h"
#include "Memory.h"

namespace
{
    LPBYTE        g_pTextBank1; // Aux
    LPBYTE        g_pTextBank0; // Main
    LPBYTE        g_pHiresBank1;
    LPBYTE        g_pHiresBank0;

    #define  SW_80COL         (g_uVideoMode & VF_80COL)
    #define  SW_DHIRES        (g_uVideoMode & VF_DHIRES)
    #define  SW_HIRES         (g_uVideoMode & VF_HIRES)
    #define  SW_80STORE       (g_uVideoMode & VF_80STORE)
    #define  SW_MIXED         (g_uVideoMode & VF_MIXED)
    #define  SW_PAGE2         (g_uVideoMode & VF_PAGE2)
    #define  SW_TEXT          (g_uVideoMode & VF_TEXT)

    bool g_bTextFlashState = false;

}

bool Video::Update40ColCell (QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    BYTE ch = *(g_pTextBank0+offset);

    const int row = ch / 16;
    const int column = ch % 16;

    const int sx = 16 * column;
    const int sy = 16 * row;
    painter.drawPixmap(x * 14, y * 16, *myCharset, sx, sy, 14, 16);

    return true;
}

bool Video::Update80ColCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    BYTE ch1 = *(g_pTextBank1+offset);
    BYTE ch2 = *(g_pTextBank0+offset);

    return true;
}

bool Video::UpdateLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    BYTE val = *(g_pTextBank0+offset);

    return true;
}

bool Video::UpdateDLoResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    return true;
}

bool Video::UpdateHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    const BYTE * base = g_pHiresBank0 + offset;

    return true;
}

bool Video::UpdateDHiResCell(QPainter & painter, int x, int y, int xpixel, int ypixel, int offset)
{
    return true;
}

Video::Video(QWidget *parent) : QWidget(parent)
{
    myCharset.reset(new QPixmap(":/resources/CHARSET4.BMP"));
}

void Video::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    const QSize min = minimumSize();
    const QSize actual = size();
    const double sx = double(actual.width()) / double(min.width());
    const double sy = double(actual.height()) / double(min.height());

    painter.scale(sx, sy);

    const int displaypage2 = (SW_PAGE2) == 0 ? 0 : 1;

    g_pHiresBank1 = MemGetAuxPtr (0x2000 << displaypage2);
    g_pHiresBank0 = MemGetMainPtr(0x2000 << displaypage2);
    g_pTextBank1  = MemGetAuxPtr (0x400  << displaypage2);
    g_pTextBank0  = MemGetMainPtr(0x400  << displaypage2);

    typedef bool (Video::*VideoUpdateFuncPtr_t)(QPainter&,int,int,int,int,int);

    VideoUpdateFuncPtr_t update = SW_TEXT
        ? SW_80COL
        ? &Video::Update80ColCell
        : &Video::Update40ColCell
        : SW_HIRES
        ? (SW_DHIRES && SW_80COL)
        ? &Video::UpdateDHiResCell
        : &Video::UpdateHiResCell
        : (SW_DHIRES && SW_80COL)
        ? &Video::UpdateDLoResCell
        : &Video::UpdateLoResCell;

    int  y        = 0;
    int  ypixel   = 0;
    while (y < 20)
    {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        int x      = 0;
        int xpixel = 0;
        while (x < 40)
        {
            (this->*update)(painter, x, y, xpixel, ypixel, offset + x);
            ++x;
            xpixel += 14;
        }
        ++y;
        ypixel += 16;
    }

    if (SW_MIXED)
        update = SW_80COL ? &Video::Update80ColCell : &Video::Update40ColCell;

    while (y < 24)
    {
        int offset = ((y & 7) << 7) + ((y >> 3) * 40);
        int x      = 0;
        int xpixel = 0;
        while (x < 40)
        {
            (this->*update)(painter, x, y, xpixel, ypixel, offset + x);
            ++x;
            xpixel += 14;
        }
        ++y;
        ypixel += 16;
    }
}
