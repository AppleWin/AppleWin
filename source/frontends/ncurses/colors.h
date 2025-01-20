#pragma once

namespace na2
{

    class GraphicsColors
    {
    public:
        GraphicsColors(const int firstColor, const int firstPair, const int numberOfGreys);

        int getPair(int color) const;
        int getGrey(double foreground, double background) const;

    private:
        int myFirstGRPair;
        int myFirstHGRPair;

        const int myNumberOfGRColors;
        const int myNumberOfGreys;
    };

} // namespace na2
