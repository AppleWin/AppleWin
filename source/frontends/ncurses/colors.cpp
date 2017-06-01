#include "frontends/ncurses/colors.h"

#include <ncurses.h>

namespace
{
  enum Color {
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
}

#define SETFRAMECOLOR(c, r, g, b) init_color(firstColor + Color::c, scaleRGB(r), scaleRGB(g), scaleRGB(b));

GraphicsColors::GraphicsColors(const int firstColor, const int firstPair)
: myFirstPair(firstPair)
{
  has_colors();
  start_color();
  can_change_color();

  SETFRAMECOLOR(BLACK,       0x00,0x00,0x00); // 0
  SETFRAMECOLOR(DEEP_RED,  0x9D,0x09,0x66); // 0xD0,0x00,0x30 -> Linards Tweaked 0x9D,0x09,0x66
  SETFRAMECOLOR(DARK_BLUE,   0x00,0x00,0x80); // 4 // not used
  SETFRAMECOLOR(MAGENTA,     0xC7,0x34,0xFF); // FD Linards Tweaked 0xFF,0x00,0xFF -> 0xC7,0x34,0xFF

  SETFRAMECOLOR(DARK_GREEN,  0x00,0x80,0x00); // 2 // not used
  SETFRAMECOLOR(DARK_GRAY,   0x80,0x80,0x80); // F8
  SETFRAMECOLOR(BLUE,        0x0D,0xA1,0xFF); // FC Linards Tweaked 0x00,0x00,0xFF -> 0x0D,0xA1,0xFF
  SETFRAMECOLOR(LIGHT_BLUE,0xAA,0xAA,0xFF); // 0x60,0xA0,0xFF -> Linards Tweaked 0xAA,0xAA,0xFF

  SETFRAMECOLOR(BROWN,     0x55,0x55,0x00); // 0x80,0x50,0x00 -> Linards Tweaked 0x55,0x55,0x00
  SETFRAMECOLOR(ORANGE,    0xF2,0x5E,0x00); // 0xFF,0x80,0x00 -> Linards Tweaked 0xF2,0x5E,0x00
  SETFRAMECOLOR(LIGHT_GRAY,  0xC0,0xC0,0xC0); // 7 // GR: COLOR=10
  SETFRAMECOLOR(PINK,      0xFF,0x89,0xE5); // 0xFF,0x90,0x80 -> Linards Tweaked 0xFF,0x89,0xE5

  SETFRAMECOLOR(GREEN,       0x38,0xCB,0x00); // FA Linards Tweaked 0x00,0xFF,0x00 -> 0x38,0xCB,0x00
  SETFRAMECOLOR(YELLOW,      0xD5,0xD5,0x1A); // FB Linards Tweaked 0xFF,0xFF,0x00 -> 0xD5,0xD5,0x1A
  SETFRAMECOLOR(AQUA,      0x62,0xF6,0x99); // 0x40,0xFF,0x90 -> Linards Tweaked 0x62,0xF6,0x99
  SETFRAMECOLOR(WHITE,       0xFF,0xFF,0xFF); // FF

  for (size_t i = 0; i < 16; ++i)
  {
    const int bg = firstColor + i;
    for (size_t j = 0; j < 16; ++j)
    {
      const int fg = firstColor + j;
      const int pair = myFirstPair + i * 16 + j;

      init_pair(pair, bg, fg);
    }
  }
}

int GraphicsColors::getPair(int color) const
{
  const int bg = color & 0x0f;
  const int fg = color >> 4;

  const int pair = myFirstPair + bg * 16 + fg;

  return pair;
}
