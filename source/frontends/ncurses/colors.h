#pragma once

class GRColors
{
 public:
  GRColors(const int firstColor, const int firstPair);

  int getPair(int color) const;

 private:
  const int myFirstPair;
};
