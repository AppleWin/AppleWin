#include "StdAfx.h"
#include "frontends/sdl/processfile.h"
#include "frontends/sdl/sdlframe.h"
#include "frontends/common2/utils.h"

#include "CardManager.h"
#include "Disk.h"
#include "Core.h"
#include "Harddisk.h"

namespace
{

  void insertDisk(sa2::SDLFrame * frame, const char * filename, const size_t dragAndDropSlot, const size_t dragAndDropDrive)
  {
    CardManager & cardManager = GetCardMgr();
    const SS_CARDTYPE cardInSlot = cardManager.QuerySlot(dragAndDropSlot);
    switch (cardInSlot)
    {
      case CT_Disk2:
      {
        Disk2InterfaceCard * card2 = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(dragAndDropSlot));
        const ImageError_e error = card2->InsertDisk(dragAndDropDrive, filename, IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
        if (error != eIMAGE_ERROR_NONE)
        {
          card2->NotifyInvalidImage(dragAndDropDrive, filename, error);
        }
        break;
      }
      case CT_GenericHDD:
      {
        if (!HD_Insert(dragAndDropDrive, filename))
        {
          frame->FrameMessageBox("Invalid HD image", "ERROR", MB_OK);
        }
        break;
      }
      default:
      {
        frame->FrameMessageBox("Invalid D&D target", "ERROR", MB_OK);
      }
    }
  }
}


namespace sa2
{

  void processFile(SDLFrame * frame, const char * filename, const size_t dragAndDropSlot, const size_t dragAndDropDrive)
  {
    const char * yaml = ".yaml";
    if (strlen(filename) > strlen(yaml) && !strcmp(filename + strlen(filename) - strlen(yaml), yaml))
    {
      common2::setSnapshotFilename(filename, true);
    }
    else
    {
      insertDisk(frame, filename, dragAndDropSlot, dragAndDropDrive);
    }
    frame->ResetSpeed();
  }
  
}
