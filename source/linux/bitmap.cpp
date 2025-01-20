#include "linux/bitmap.h"

#include <stdexcept>
#include <cstring>
#include <cstdint>

namespace
{

    template <typename T> T getAs(const unsigned char *buffer, const size_t bufferSize, const size_t offset)
    {
        if (offset + sizeof(T) > bufferSize)
        {
            throw std::runtime_error("Invalid bitmap");
        }
        const T *ptr = reinterpret_cast<const T *>(buffer + offset);
        return *ptr;
    }

    // minimal parser to extract data from the AppleWin BPMs.
    bool extractBitmapData(
        const unsigned char *buffer, const size_t bufferSize, int32_t &width, int32_t &height, uint16_t &bpp,
        const unsigned char *&data, uint32_t &size)
    {
        if (bufferSize < 2)
        {
            return false;
        }

        if (buffer[0] != 0x42 || buffer[1] != 0x4D)
        {
            return false;
        }

        const uint32_t fileSize = getAs<uint32_t>(buffer, bufferSize, 2);
        if (fileSize != bufferSize)
        {
            return false;
        }

        const uint32_t offset = getAs<uint32_t>(buffer, bufferSize, 10);
        const uint32_t header = getAs<uint32_t>(buffer, bufferSize, 14);
        if (header != 40)
        {
            return false;
        }

        width = getAs<int32_t>(buffer, bufferSize, 18);
        height = getAs<int32_t>(buffer, bufferSize, 22);
        bpp = getAs<uint16_t>(buffer, bufferSize, 28);
        const uint32_t imageSize = getAs<uint32_t>(buffer, bufferSize, 34);
        if (offset + imageSize > fileSize)
        {
            return false;
        }
        data = buffer + offset;
        size = imageSize;
        return true;
    }

} // namespace

bool GetBitmapFromResource(const std::pair<const unsigned char *, unsigned int> &resource, int cb, void *lpvBits)
{
    int32_t width, height;
    uint16_t bpp;
    const unsigned char *data;
    uint32_t size;
    const bool res = extractBitmapData(resource.first, resource.second, width, height, bpp, data, size);

    if (res && bpp == 1 && height > 0 && width > 0 && size <= cb)
    {
        const size_t length = size / height;
        // rows are stored upside down
        unsigned char *out = static_cast<unsigned char *>(lpvBits);
        for (size_t row = 0; row < height; ++row)
        {
            const unsigned char *src = data + row * length;
            unsigned char *dst = out + (height - row - 1) * length;
            memcpy(dst, src, length);
        }
        return true;
    }

    return false;
}
