#include "frontends/ncurses/colors.h"

#include "frontends/ncurses/ncurses_safe.h"
#include <cmath>

namespace
{
    enum Color
    {
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

    // input 0..255
    // output 0..1000
    int scaleRGB(int rgb)
    {
        return rgb * 1000 / 255;
    }
} // namespace

#define SETFRAMECOLOR(c, r, g, b) init_color(firstColor + Color::c, scaleRGB(r), scaleRGB(g), scaleRGB(b));

namespace na2
{

    GraphicsColors::GraphicsColors(const int firstColor, const int firstPair, const int numberOfGreys)
        : myNumberOfGRColors(16)
        , myNumberOfGreys(numberOfGreys)
    {
        has_colors();
        start_color();
        can_change_color();

        SETFRAMECOLOR(BLACK, 0x00, 0x00, 0x00);     // 0
        SETFRAMECOLOR(DEEP_RED, 0x9D, 0x09, 0x66);  // 0xD0,0x00,0x30 -> Linards Tweaked 0x9D,0x09,0x66
        SETFRAMECOLOR(DARK_BLUE, 0x00, 0x00, 0x80); // 4 // not used
        SETFRAMECOLOR(MAGENTA, 0xC7, 0x34, 0xFF);   // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF

        SETFRAMECOLOR(DARK_GREEN, 0x00, 0x80, 0x00); // 2 // not used
        SETFRAMECOLOR(DARK_GRAY, 0x80, 0x80, 0x80);  // F8
        SETFRAMECOLOR(BLUE, 0x0D, 0xA1, 0xFF);       // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
        SETFRAMECOLOR(LIGHT_BLUE, 0xAA, 0xAA, 0xFF); // 0x60,0xA0,0xFF -> Linards Tweaked 0xAA,0xAA,0xFF

        SETFRAMECOLOR(BROWN, 0x55, 0x55, 0x00);      // 0x80,0x50,0x00 -> Linards Tweaked 0x55,0x55,0x00
        SETFRAMECOLOR(ORANGE, 0xF2, 0x5E, 0x00);     // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
        SETFRAMECOLOR(LIGHT_GRAY, 0xC0, 0xC0, 0xC0); // 7 // GR: COLOR=10
        SETFRAMECOLOR(PINK, 0xFF, 0x89, 0xE5);       // 0xFF,0x90,0x80 -> Linards Tweaked 0xFF,0x89,0xE5

        SETFRAMECOLOR(GREEN, 0x38, 0xCB, 0x00);  // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
        SETFRAMECOLOR(YELLOW, 0xD5, 0xD5, 0x1A); // FB Linards Tweaked 0xFF,0xFF,0x00 -> 0xD5,0xD5,0x1A
        SETFRAMECOLOR(AQUA, 0x62, 0xF6, 0x99);   // 0x40,0xFF,0x90 -> Linards Tweaked 0x62,0xF6,0x99
        SETFRAMECOLOR(WHITE, 0xFF, 0xFF, 0xFF);  // FF

        int baseColor = firstColor;
        int basePair = firstPair;
        myFirstGRPair = basePair;

        for (size_t i = 0; i < myNumberOfGRColors; ++i)
        {
            const int fg = firstColor + i;
            for (size_t j = 0; j < myNumberOfGRColors; ++j)
            {
                const int bg = firstColor + j;
                const int pair = myFirstGRPair + i * myNumberOfGRColors + j;

                init_pair(pair, fg, bg);
            }
        }

        baseColor += myNumberOfGRColors;
        basePair += myNumberOfGRColors * myNumberOfGRColors;
        myFirstHGRPair = basePair;

        for (size_t i = 0; i < myNumberOfGreys; ++i)
        {
            const int color = baseColor + i;
            const int grey = 1000 * i / (myNumberOfGreys - 1);
            init_color(color, grey, grey, grey);
        }

        for (size_t i = 0; i < myNumberOfGreys; ++i)
        {
            const int fg = baseColor + i;
            for (size_t j = 0; j < myNumberOfGreys; ++j)
            {
                const int bg = baseColor + j;
                const int pair = basePair + i * myNumberOfGreys + j;

                init_pair(pair, fg, bg);
            }
        }
    }

    int GraphicsColors::getPair(int color) const
    {
        const int fg = color & 0x0f;
        const int bg = color >> 4;

        const int pair = myFirstGRPair + fg * myNumberOfGRColors + bg;

        return pair;
    }

    int GraphicsColors::getGrey(double foreground, double background) const
    {
        const int fg = std::nearbyint((myNumberOfGreys - 1) * foreground);
        const int bg = std::nearbyint((myNumberOfGreys - 1) * background);

        const int basePair = myFirstHGRPair;

        const int pair = basePair + fg * myNumberOfGreys + bg;

        return pair;
    }

} // namespace na2
