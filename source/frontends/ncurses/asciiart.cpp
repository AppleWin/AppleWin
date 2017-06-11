#include "frontends/ncurses/asciiart.h"

#include <cfloat>
#include <memory>

ASCIIArt::Unicode::Unicode(const char * aC, std::vector<int> aValues)
  : c(aC), values(aValues)
{
}

const int ASCIIArt::PPQ = 8 * (2 * 7) / 4;

ASCIIArt::ASCIIArt()
{
  myGlyphs.push_back(Unicode("\u2580", {PPQ, PPQ,   0,   0}));  // top half
  myGlyphs.push_back(Unicode("\u258C", {PPQ,   0, PPQ,   0}));  // left half
  myGlyphs.push_back(Unicode("\u2596", {  0,   0, PPQ,   0}));  // lower left
  myGlyphs.push_back(Unicode("\u2597", {  0,   0,   0, PPQ}));  // lower right
  myGlyphs.push_back(Unicode("\u2598", {PPQ,   0,   0,   0}));  // top left
  myGlyphs.push_back(Unicode("\u259A", {PPQ,   0,   0, PPQ}));  // diagonal
  myGlyphs.push_back(Unicode("\u259D", {  0, PPQ,   0,   0}));  // top right
}

const ASCIIArt::Character & ASCIIArt::getCharacter(const unsigned char * address)
{
  std::vector<int> values(4, 0);

  // 8 lines per text character
  for (size_t i = 0; i < 8; ++i)
  {
    const int offset = 0x0400 * i;
    const unsigned char value = *(address + offset);

    const int base = (i / 4) * 2;

    //                                               76543210
    values[base]     += __builtin_popcount(value & 0b00000111) * 2;
    values[base + 1] += __builtin_popcount(value & 0b01110000) * 2;

    // allocate the middle pixels to each quadrant (with half the weight)
    if (value & 0b00001000)
    {
      ++values[base];
      ++values[base + 1];
    }
  }

  return getCharacter(values);
}

const ASCIIArt::Character & ASCIIArt::getCharacter(const std::vector<int> & values)
{
  int zip = 0;
  for (const int v: values)
  {
    zip = (zip << 4) + v;
  }

  const std::unordered_map<int, Character>::const_iterator it = myAsciiPixels.find(zip);
  if (it == myAsciiPixels.end())
  {
    Character & best = myAsciiPixels[zip];
    best.error = DBL_MAX;

    for (const Unicode & glyph: myGlyphs)
    {
      double foreground;
      double background;
      double error;
      fit(values, glyph, foreground, background, error);
      if (error < best.error)
      {
	best.error = error;
	best.foreground = foreground;
	best.background = background;
	best.c = glyph.c;
      }
    }

    return best;
  }
  else
  {
    return it->second;
  }

}

void ASCIIArt::fit(const std::vector<int> & art, const Unicode & glyph,
		   double & foreground, double & background, double & error)
{
  int num_fg = 0;
  int num_bg = 0;
  int den_fg = 0;
  int den_bg = 0;

  for (size_t i = 0; i < art.size(); ++i)
  {
    const double f = glyph.values[i];
    const double b = PPQ - f;

    num_fg += art[i] * f;
    den_fg += f * f;

    num_bg += art[i] * b;
    den_bg += b * b;
  }

  // close formula to minimise the L2 norm of the difference
  // of grey intensity
  foreground = double(num_fg) / double(den_fg);
  background = double(num_bg) / double(den_bg);

  error = 0.0;
  for (size_t i = 0; i < art.size(); ++i)
  {
    const double f = glyph.values[i];
    const double b = PPQ - f;

    const double g = foreground * f + background * b;
    const double e = art[i] - g;
    error += e * e;
  }
}
