#include "frontends/ncurses/asciiart.h"

#include <cfloat>
#include <memory>

namespace na2
{

    ASCIIArt::Unicode::Unicode(const char *aC, Blocks aValues)
        : c(aC)
        , values(aValues)
    {
    }

    const int ASCIIArt::PPQ = 8 * (2 * 7) / 4;

    ASCIIArt::ASCIIArt()
        : myRows(0)
        , myColumns(0)
    {
        // clang-format off
        myGlyphs.push_back(Unicode("\u2580", {PPQ, PPQ,   0,   0}));  // top half
        myGlyphs.push_back(Unicode("\u258C", {PPQ,   0, PPQ,   0}));  // left half
        myGlyphs.push_back(Unicode("\u2596", {  0,   0, PPQ,   0}));  // lower left
        myGlyphs.push_back(Unicode("\u2597", {  0,   0,   0, PPQ}));  // lower right
        myGlyphs.push_back(Unicode("\u2598", {PPQ,   0,   0,   0}));  // top left
        myGlyphs.push_back(Unicode("\u259A", {PPQ,   0,   0, PPQ}));  // diagonal
        myGlyphs.push_back(Unicode("\u259D", {  0, PPQ,   0,   0}));  // top right
        // clang-format on

        myBlocks.resize(128);

        init(1, 1); // normal size
    }

    void ASCIIArt::init(const int rows, const int columns)
    {
        if (myRows != rows || myColumns != columns)
        {
            if (myColumns != columns)
            {
                myColumns = columns;
                for (size_t i = 0; i < myBlocks.size(); ++i)
                {
                    myBlocks[i] = decodeByte(i);
                }
            }

            myRows = rows;

            myValues.resize(boost::extents[myRows][myColumns]);
            myChars.resize(boost::extents[rows][columns]);
        }
    }

    void ASCIIArt::changeColumns(const int x)
    {
        int newColumns = myColumns + x;
        newColumns = std::max(1, std::min(7, newColumns));
        init(myRows, newColumns);
    }

    void ASCIIArt::changeRows(const int x)
    {
        int newRows = x > 0 ? myRows * 2 : myRows / 2;
        newRows = std::max(1, std::min(4, newRows));
        init(newRows, myColumns);
    }

    const ASCIIArt::array_char_t &ASCIIArt::getCharacters(const unsigned char *address)
    {
        const array_val_t &values = getQuadrantValues(address);
        return getCharacters(values);
    }

    const ASCIIArt::array_val_t &ASCIIArt::getQuadrantValues(const unsigned char *address) const
    {
        std::fill(myValues.origin(), myValues.origin() + myValues.num_elements(), 0);

        const int linesPerRow = 8 / myRows;
        const int linesPerQuadrant = linesPerRow / 2;

        // 8 lines per text character
        for (size_t i = 0; i < 8; ++i)
        {
            const int offset = 0x0400 * i;
            // group color bit is ignored
            const unsigned char value = (*(address + offset)) & 0x7f;

            const int row = i / linesPerRow;
            const int lineInRow = i % linesPerRow;
            const int quadrant = lineInRow / linesPerQuadrant;
            const int base = quadrant * 2;

            const std::vector<Blocks> &decoded = myBlocks[value];

            for (size_t col = 0; col < myColumns; ++col)
            {
                Blocks &blocks = myValues[row][col];
                blocks.add(base + 0, decoded[col][0] * myRows);
                blocks.add(base + 1, decoded[col][1] * myRows);
            }
        }

        return myValues;
    }

    std::vector<Blocks> ASCIIArt::decodeByte(const unsigned char value) const
    {
        const int each = myColumns * 4 * PPQ / (8 * 7);

        int available = 7;
        int col = 0;
        int pos = 0; // left right

        std::vector<Blocks> decoded(myColumns);

        for (size_t j = 0; j < 7; ++j)
        {
            int to_allocate = each;
            do
            {
                const int here = std::min(available, to_allocate);
                if (value & (1 << j))
                {
                    decoded[col].add(pos, here);
                }
                to_allocate -= here;
                available -= here;
                if (available == 0)
                {
                    // new quadrant
                    available = 7;
                    ++pos;
                    if (pos == 2)
                    {
                        pos = 0;
                        ++col;
                    }
                }
            } while (to_allocate > 0);
        }

        return decoded;
    }

    const ASCIIArt::array_char_t &ASCIIArt::getCharacters(const array_val_t &values)
    {
        const int rows = values.shape()[0];
        const int columns = values.shape()[1];

        for (size_t i = 0; i < rows; ++i)
        {
            for (size_t j = 0; j < columns; ++j)
            {
                myChars[i][j] = getCharacter(values[i][j]);
            }
        }

        return myChars;
    }

    const ASCIIArt::Character &ASCIIArt::getCharacter(const Blocks &values)
    {
        const int zip = values.value;

        const std::unordered_map<int, Character>::const_iterator it = myAsciiPixels.find(zip);
        if (it == myAsciiPixels.end())
        {
            Character &best = myAsciiPixels[zip];
            best.error = DBL_MAX;

            for (const Unicode &glyph : myGlyphs)
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

    void ASCIIArt::fit(const Blocks &art, const Unicode &glyph, double &foreground, double &background, double &error)
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

} // namespace na2
