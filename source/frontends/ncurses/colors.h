#pragma once

class GraphicsColors
{
 public:
  GraphicsColors(const int firstColor, const int firstPair);

  int getPair(int color) const;

 private:
  const int myFirstPair;
};
