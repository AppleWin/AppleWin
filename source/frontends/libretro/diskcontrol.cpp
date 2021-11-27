#include "StdAfx.h"
#include "frontends/libretro/diskcontrol.h"
#include "frontends/libretro/environment.h"

#include "Core.h"
#include "CardManager.h"
#include "Disk.h"
#include "Harddisk.h"

namespace ra2
{

  DiskControl::DiskControl() : myEjected(false), myIndex(0)
  {

  }

  bool DiskControl::insertDisk(const std::string & filename)
  {
    if (filename.empty())
    {
      return false;
    }

    if (insertFloppyDisk(filename))
    {
      myIndex = 0;
      myImages.clear();
      myImages.push_back(filename);
      myEjected = false;
      return true;
    }

    return insertHardDisk(filename);
  }

  bool DiskControl::insertFloppyDisk(const std::string & filename) const
  {
    CardManager & cardManager = GetCardMgr();

    Disk2InterfaceCard * disk2Card = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6));
    if (disk2Card)
    {
      const ImageError_e error = disk2Card->InsertDisk(DRIVE_1, filename.c_str(), IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE);

      if (error == eIMAGE_ERROR_NONE)
      {
        return true;
      }
    }

    return false;
  }

  bool DiskControl::insertHardDisk(const std::string & filename) const
  {
    CardManager & cardManager = GetCardMgr();

    if (cardManager.QuerySlot(SLOT7) != CT_GenericHDD)
    {
      cardManager.Insert(SLOT7, CT_GenericHDD);
    }

    HarddiskInterfaceCard * harddiskCard = dynamic_cast<HarddiskInterfaceCard*>(cardManager.GetObj(SLOT7));
    if (harddiskCard)
    {
      BOOL bRes = harddiskCard->Insert(HARDDISK_1, filename);
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
      return true;
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
        result = insertFloppyDisk(myImages[myIndex]);
        myEjected = !result;
        ra2::log_cb(RETRO_LOG_INFO, "Insert new disk: %s -> %d\n", myImages[myIndex].c_str(), result);
      }
    }

    return result;
  }

  size_t DiskControl::getImageIndex() const
  {
    return myIndex;
  }

  void DiskControl::checkState() const
  {
    if (!myEjected)
    {
      // this should not happen
      ra2::log_cb(RETRO_LOG_INFO, "WTF\n");
    }
  }

  bool DiskControl::setImageIndex(size_t index)
  {
    checkState();
    myIndex = index;
    return true;
  }

  size_t DiskControl::getNumImages() const
  {
    return myImages.size();
  }

  bool DiskControl::replaceImageIndex(size_t index, const std::string & path)
  {
    if (myIndex < myImages.size())
    {
      myImages[index] = path;
      return true;
    }
    else
    {
      return false;
    }
  }

  bool DiskControl::removeImageIndex(size_t index)
  {
    if (myIndex < myImages.size())
    {
      myImages.erase(myImages.begin() + index);
      if (myIndex == index)
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
    myImages.push_back(std::string());
    return true;
  }

}
