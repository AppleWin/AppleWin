#pragma once

#include <unordered_map>
#include <vector>
#include <initializer_list>
#include <boost/multi_array.hpp>

namespace na2
{

    struct Blocks
    {
        Blocks()
            : value(0)
        {
        }

        Blocks(std::initializer_list<int> q)
        {
            value = 0;
            for (auto it = rbegin(q); it != rend(q); ++it)
            {
                value <<= 5;
                value += *it;
            }
        }

        void add(int i, int val)
        {
            value += val << (i * 5);
        }

        int operator[](int i) const
        {
            const int val = (value >> (i * 5)) & 0b11111;
            return val;
        }

        Blocks &operator=(const int val)
        {
            value = val;
            return *this;
        }

        size_t size() const
        {
            return 4;
        }

        int value;
    };

    class ASCIIArt
    {
    public:
        ASCIIArt();

        struct Character
        {
            const char *c;
            double foreground;
            double background;
            double error;
        };

        typedef boost::multi_array<Character, 2> array_char_t;
        typedef boost::multi_array<Blocks, 2> array_val_t;

        void init(const int rows, const int columns);

        void changeColumns(const int x);
        void changeRows(const int x);

        const array_char_t &getCharacters(const unsigned char *address);
        const array_char_t &getCharacters(const array_val_t &values);
        const array_val_t &getQuadrantValues(const unsigned char *address) const;
        std::vector<Blocks> decodeByte(const unsigned char value) const;

    private:
        static const int PPQ; // Pixels per Quadrant

        std::unordered_map<int, Character> myAsciiPixels;

        struct Unicode
        {
            Unicode(const char *aC, Blocks aValues);

            const char *c;
            Blocks values; // foreground: top left - top right - bottom left - bottom right
        };

        int myRows;
        int myColumns;

        std::vector<Unicode> myGlyphs;

        std::vector<std::vector<Blocks>> myBlocks;

        mutable array_val_t myValues; // workspace
        array_char_t myChars;         // workspace

        const Character &getCharacter(const Blocks &values);
        static void fit(const Blocks &art, const Unicode &glyph, double &foreground, double &background, double &error);
    };

} // namespace na2
