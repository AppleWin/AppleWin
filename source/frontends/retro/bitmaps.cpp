#include "frontends/retro/environment.h"

#include <linux/interface.h>
#include <linux/windows/misc.h>
#include <frontends/common2/resources.h>

#include <fstream>
#include <cstring>

namespace
{

  void readFileToBuffer(const std::string & filename, std::vector<char> & buffer)
  {
    std::ifstream file(filename.c_str(), std::ios::binary | std::ios::ate);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer.resize(size);
    file.read(buffer.data(), size);
  }

  template<typename T>
  T getAs(const std::vector<char> & buffer, const size_t offset)
  {
    if (offset + sizeof(T) > buffer.size())
    {
      throw std::runtime_error("Invalid bitmap");
    }
    const T * ptr = reinterpret_cast<const T *>(buffer.data() + offset);
    return * ptr;
  }

  // libretro cannot parse BMP with 1 bpp
  // simple version implemented here
  bool getBitmapData(const std::vector<char> & buffer, int32_t & width, int32_t & height, uint16_t & bpp, const char * & data, uint32_t & size)
  {
    if (buffer.size() < 2)
    {
      return false;
    }

    if (buffer[0] != 0x42 || buffer[1] != 0x4D)
    {
      return false;
    }

    const uint32_t fileSize = getAs<uint32_t>(buffer, 2);
    if (fileSize != buffer.size())
    {
      return false;
    }

    const uint32_t offset = getAs<uint32_t>(buffer, 10);
    const uint32_t header = getAs<uint32_t>(buffer, 14);
    if (header != 40)
    {
      return false;
    }

    width = getAs<int32_t>(buffer, 18);
    height = getAs<int32_t>(buffer, 22);
    bpp = getAs<uint16_t>(buffer, 28);
    const uint32_t imageSize = getAs<uint32_t>(buffer, 34);
    if (offset + imageSize > fileSize)
    {
      return false;
    }
    data = buffer.data() + offset;
    size = imageSize;
    return true;
  }

}

struct CBITMAP : public CHANDLE
{
  std::vector<char> image;
};

std::string getFilename(const std::string & resource)
{
  if (resource == "CHARSET40") return "CHARSET4.BMP";
  if (resource == "CHARSET82") return "CHARSET82.bmp";
  if (resource == "CHARSET8M") return "CHARSET8M.bmp";
  if (resource == "CHARSET8C") return "CHARSET8C.bmp";

  return resource;
}

HBITMAP LoadBitmap(HINSTANCE hInstance, const char * resource)
{
  if (resource)
  {
    const std::string filename = getFilename(resource);
    const std::string path = getResourcePath() + filename;

    std::vector<char> buffer;
    readFileToBuffer(path, buffer);

    if (!buffer.empty())
    {
      int32_t width, height;
      uint16_t bpp;
      const char * data;
      uint32_t size;
      const bool res = getBitmapData(buffer, width, height, bpp, data, size);

      log_cb(RETRO_LOG_INFO, "RA2: %s. %s = %dx%d, %dbpp\n", __FUNCTION__, path.c_str(),
	     width, height, bpp);

      if (res && height > 0)
      {
	CBITMAP * bitmap = new CBITMAP;
	bitmap->image.resize(size);
	const size_t length = size / height;
	// rows are stored upside down
	for (size_t row = 0; row < height; ++row)
	{
	  const char * src = data + row * length;
	  char * dst = bitmap->image.data() + (height - row - 1) * length;
	  memcpy(dst, src, length);
	}
	return bitmap;
      }
    }
    else
    {
      log_cb(RETRO_LOG_INFO, "RA2: %s. Cannot load: %s with path: %s\n", __FUNCTION__, resource, path.c_str());
      return nullptr;
    }
  }

  log_cb(RETRO_LOG_INFO, "RA2: %s. Cannot load invaid resource\n", __FUNCTION__);
  return nullptr;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
  const CBITMAP & bitmap = dynamic_cast<CBITMAP&>(*hbit);
  const char * bits = bitmap.image.data();
  const size_t size = bitmap.image.size();
  const size_t requested = cb;

  const size_t copied = std::min(requested, size);

  char * dest = static_cast<char *>(lpvBits);
  memcpy(dest, bits, copied);

  return copied;
}
