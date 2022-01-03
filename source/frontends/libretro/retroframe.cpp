#include "StdAfx.h"
#include "frontends/libretro/retroframe.h"
#include "frontends/libretro/environment.h"

#include "Interface.h"
#include "Core.h"
#include "Utilities.h"

#include <fstream>

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

namespace ra2
{

  RetroFrame::RetroFrame()
  {
  }

  void RetroFrame::FrameRefreshStatus(int drawflags)
  {
    if (drawflags & DRAW_TITLE)
    {
      GetAppleWindowTitle();
      display_message(g_pAppTitle.c_str());
    }
  }

  void RetroFrame::VideoPresentScreen()
  {
    // this should not be necessary
    // either libretro handles it
    // or we should change AW
    // but for now, there is no alternative
    for (size_t row = 0; row < myHeight; ++row)
    {
      const uint8_t * src = myFrameBuffer + row * myPitch;
      uint8_t * dst = myVideoBuffer.data() + (myHeight - row - 1) * myPitch;
      memcpy(dst, src, myPitch);
    }

    video_cb(myVideoBuffer.data() + myOffset, myBorderlessWidth, myBorderlessHeight, myPitch);
  }

  void RetroFrame::Initialize(bool resetVideoState)
  {
    CommonFrame::Initialize(resetVideoState);
    FrameRefreshStatus(DRAW_TITLE);

    Video & video = GetVideo();

    myBorderlessWidth = video.GetFrameBufferBorderlessWidth();
    myBorderlessHeight = video.GetFrameBufferBorderlessHeight();
    const size_t borderWidth = video.GetFrameBufferBorderWidth();
    const size_t borderHeight = video.GetFrameBufferBorderHeight();
    const size_t width = video.GetFrameBufferWidth();
    myHeight = video.GetFrameBufferHeight();

    myFrameBuffer = video.GetFrameBuffer();

    myPitch = width * sizeof(bgra_t);
    myOffset = (width * borderHeight + borderWidth) * sizeof(bgra_t);

    const size_t size = myHeight * myPitch;
    myVideoBuffer.resize(size);
  }

  void RetroFrame::Destroy()
  {
    CommonFrame::Destroy();
    myFrameBuffer = nullptr;
    myVideoBuffer.clear();
  }

  void RetroFrame::GetBitmap(LPCSTR lpBitmapName, LONG cb, LPVOID lpvBits)
  {
    const std::string filename = getBitmapFilename(lpBitmapName);
    const std::string path = getResourcePath(filename);

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

      if (res && height > 0 && size <= cb)
      {
        const size_t length = size / height;
        // rows are stored upside down
        char * out = static_cast<char *>(lpvBits);
        for (size_t row = 0; row < height; ++row)
        {
          const char * src = data + row * length;
          char * dst = out + (height - row - 1) * length;
          memcpy(dst, src, length);
        }
        return;
      }
    }

    CommonFrame::GetBitmap(lpBitmapName, cb, lpvBits);
  }

  int RetroFrame::FrameMessageBox(LPCSTR lpText, LPCSTR lpCaption, UINT uType)
  {
    log_cb(RETRO_LOG_INFO, "RA2: %s: %s - %s\n", __FUNCTION__, lpCaption, lpText);
    return IDOK;
  }

}
