#pragma once

#include <unordered_map>
#include <vector>

class ASCIIArt
{
public:
  ASCIIArt();

  struct Character
  {
    const char * c;
    double foreground;
    double background;
    double error;
  };

  const Character & getCharacter(const unsigned char * address);
  const Character & getCharacter(const std::vector<int> & values);

private:
  std::unordered_map<int, Character> myAsciiPixels;

  struct Unicode
  {
    Unicode(const char * aC, std::vector<int> aValues);

    const char * c;
    std::vector<int> values; // foreground: top left - top right - bottom left - bottom right
  };

  std::vector<Unicode> myGlyphs;

  static void fit(const std::vector<int> & art, const Unicode & glyph,
		  double & foreground, double & background, double & error);

};
