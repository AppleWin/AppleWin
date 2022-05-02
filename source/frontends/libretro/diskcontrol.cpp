#include "StdAfx.h"
#include "frontends/libretro/diskcontrol.h"
#include "frontends/libretro/environment.h"

#include "Core.h"
#include "CardManager.h"
#include "Disk.h"
#include "Harddisk.h"

#include <fstream>
#include <filesystem>


namespace
{

  const std::string M3U_COMMENT("#");
  const std::string M3U_SAVEDISK("#SAVEDISK:");
  const std::string M3U_SAVEDISK_LABEL("Save Disk ");

  bool startsWith(const std::string & value, const std::string & prefix)
  {
    if (prefix.size() > value.size())
    {
      return false;
    }
    return std::equal(prefix.begin(), prefix.end(), value.begin());
  }

  void getLabelAndPath(const std::string & line, std::filesystem::path & path, std::string & label)
  {
    const size_t pos = line.find('|');
    if (pos == std::string::npos)
    {
      path = line;
      label = path.stem();
    }
    else
    {
      path = line.substr(0, pos);
      label = line.substr(pos + 1);
    }
  }

  void writeString(ra2::Buffer<char> & buffer, const std::string & value)
  {
    size_t const size = value.size();
    buffer.get<size_t>() = size;
    char * begin, * end;
    buffer.get(size, begin, end);
    memcpy(begin, value.data(), end - begin);
  }

  void readString(ra2::Buffer<const char> & buffer, std::string & value)
  {
    size_t const size = buffer.get<size_t const>();
    char const * begin, * end;
    buffer.get(size, begin, end);
    value.assign(begin, end);
  }

}

namespace ra2
{

  DiskControl::DiskControl() : myEjected(false), myIndex(0)
  {

  }

  bool DiskControl::insertDisk(const std::string & path)
  {
    const bool writeProtected = IMAGE_FORCE_WRITE_PROTECTED;
    const bool createIfNecessary = IMAGE_DONT_CREATE;

    if (insertFloppyDisk(path, writeProtected, createIfNecessary))
    {
      myIndex = 0;
      myImages.clear();

      const std::filesystem::path filePath(path);
      myImages.push_back({filePath.native(), filePath.stem(), writeProtected, createIfNecessary});
      myEjected = false;
      return true;
    }

    return insertHardDisk(path);
  }

  bool DiskControl::insertPlaylist(const std::string & path)
  {
    const std::filesystem::path playlistPath(path);
    std::ifstream playlist(playlistPath);
    if (!playlist)
    {
      return false;
    }

    myImages.clear();
    const std::filesystem::path parent = playlistPath.parent_path();
    const std::filesystem::path savePath(ra2::save_directory);
    const std::string playlistStem = playlistPath.stem();

    std::string line;
    while (std::getline(playlist, line))
    {
      // should we trim initial spaces?
      if (startsWith(line, M3U_SAVEDISK))
      {
        const size_t index = myImages.size() + 1;
        const std::string filename = StrFormat("%s.save%" SIZE_T_FMT ".dsk", playlistStem.c_str(), index);

        // 1) use the label from #SAVEDISK:label
        std::string labelSuffix = line.substr(M3U_SAVEDISK.size());
        if (labelSuffix.empty())
        {
          // 2) use an automatic label "the index"
          labelSuffix = std::to_string(index);
        }

        // Always prefix with "Save Disk"
        const std::string label = M3U_SAVEDISK_LABEL + labelSuffix;

        const std::filesystem::path imagePath = savePath / filename;

        // TODO: this disk is NOT formatted
        myImages.push_back({imagePath.native(), label, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_CREATE});
      }
      else if (!startsWith(line, M3U_COMMENT))
      {
        std::filesystem::path imagePath;
        std::string label;
        getLabelAndPath(line, imagePath, label);

        if (imagePath.is_relative())
        {
          imagePath = parent / imagePath;
        }
        myImages.push_back({imagePath.native(), label, IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE});
      }
    }

    // if we have an initial disk image, let's try to honour it
    if (!ourInitialPath.empty() && ourInitialIndex < myImages.size() && myImages[ourInitialIndex].path == ourInitialPath)
    {
      myIndex = ourInitialIndex;
      // do we need to reset for next time?
      ourInitialPath.clear();
      ourInitialIndex = 0;
    }
    else
    {
      // insert the first image
      myIndex = 0;
    }

    // this is safe even if myImages is empty
    myEjected = true;
    return setEjectedState(false);
  }

  bool DiskControl::insertFloppyDisk(const std::string & path, const bool writeProtected, bool const createIfNecessary) const
  {
    CardManager & cardManager = GetCardMgr();

    Disk2InterfaceCard * disk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));
    if (disk2Card)
    {
      const ImageError_e error = disk2Card->InsertDisk(DRIVE_1, path, writeProtected, createIfNecessary);

      if (error == eIMAGE_ERROR_NONE)
      {
        return true;
      }
    }

    return false;
  }

  bool DiskControl::insertHardDisk(const std::string & path) const
  {
    CardManager & cardManager = GetCardMgr();

    if (cardManager.QuerySlot(SLOT7) != CT_GenericHDD)
    {
      cardManager.Insert(SLOT7, CT_GenericHDD);
    }

    HarddiskInterfaceCard * harddiskCard = dynamic_cast<HarddiskInterfaceCard*>(cardManager.GetObj(SLOT7));
    if (harddiskCard)
    {
      BOOL bRes = harddiskCard->Insert(HARDDISK_1, path);
      return bRes == TRUE;
    }

    return false;
  }

  bool DiskControl::getEjectedState() const
  {
    return myEjected;
  }

  bool DiskControl::setEjectedState(bool ejected)
  {
    if (myEjected == ejected)
    {
      return true; // or false?
    }

    CardManager & cardManager = GetCardMgr();
    Disk2InterfaceCard * disk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));

    bool result = false;
    if (disk2Card && myIndex < myImages.size())
    {
      if (ejected)
      {
        disk2Card->EjectDisk(DRIVE_1);
        result = true;
        myEjected = ejected;
      }
      else
      {
        // inserted
        result = insertFloppyDisk(myImages[myIndex].path, myImages[myIndex].writeProtected, myImages[myIndex].createIfNecessary);
        myEjected = !result;
        ra2::log_cb(RETRO_LOG_INFO, "Insert new disk: %s -> %d\n", myImages[myIndex].path.c_str(), result);
      }
    }

    return result;
  }

  size_t DiskControl::getImageIndex() const
  {
    return myIndex;
  }

  bool DiskControl::setImageIndex(size_t index)
  {
    if (myEjected)
    {
      myIndex = index;
      return true;
    }
    else
    {
      return false;
    }
  }

  size_t DiskControl::getNumImages() const
  {
    return myImages.size();
  }

  bool DiskControl::replaceImageIndex(size_t index, const std::string & path)
  {
    if (myEjected && myIndex < myImages.size())
    {
      const std::filesystem::path filePath(path);

      myImages[index].path = filePath.native();
      myImages[index].label = filePath.stem();
      myImages[index].writeProtected = IMAGE_FORCE_WRITE_PROTECTED;
      myImages[index].createIfNecessary = IMAGE_DONT_CREATE;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DiskControl::removeImageIndex(size_t index)
  {
    if (myEjected && myIndex < myImages.size())
    {
      myImages.erase(myImages.begin() + index);
      if (myImages.empty() || myIndex == index)
      {
        myIndex = myImages.size();
      }
      else if (myIndex > index)
      {
        --myIndex;
      }
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DiskControl::addImageIndex()
  {
    myImages.push_back({"", "", IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE});
    return true;
  }

  bool DiskControl::getImagePath(unsigned index, char *path, size_t len) const
  {
    if (index < myImages.size())
    {
      strncpy(path, myImages[index].path.c_str(), len);
      path[len - 1] = 0;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DiskControl::getImageLabel(unsigned index, char *label, size_t len) const
  {
    if (index < myImages.size())
    {
      strncpy(label, myImages[index].label.c_str(), len);
      label[len - 1] = 0;
      return true;
    }
    else
    {
      return false;
    }
  }

  void DiskControl::serialise(Buffer<char> & buffer) const
  {
    buffer.get<bool>() = myEjected;
    buffer.get<size_t>() = myIndex;
    buffer.get<size_t>() = myImages.size();

    for (DiskInfo const & image : myImages)
    {
      writeString(buffer, image.path);
      writeString(buffer, image.label);
      buffer.get<bool>() = image.writeProtected;
      buffer.get<bool>() = image.createIfNecessary;
    }
  }

  void DiskControl::deserialise(Buffer<char const> & buffer)
  {
    myEjected = buffer.get<bool const>();
    myIndex = buffer.get<size_t const>();
    size_t const numberOfImages = buffer.get<size_t const>();
    myImages.clear();
    myImages.resize(numberOfImages);

    for (DiskInfo & image : myImages)
    {
      readString(buffer, image.path);
      readString(buffer, image.label);
      image.writeProtected = buffer.get<const bool>();
      image.createIfNecessary = buffer.get<const bool>();
    }
  }

  unsigned DiskControl::ourInitialIndex = 0;
  std::string DiskControl::ourInitialPath;

  void DiskControl::setInitialPath(unsigned index, const char *path)
  {
    if (path && *path)
    {
      ourInitialIndex = index;
      ourInitialPath = path;
    }
  }

}
