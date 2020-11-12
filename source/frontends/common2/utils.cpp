#include <frontends/common2/utils.h>

#include "StdAfx.h"
#include "Common.h"
#include "AppleWin.h"
#include "CardManager.h"
#include "Disk.h"
#include "SaveState.h"

#include <libgen.h>
#include <unistd.h>

bool DoDiskInsert(const UINT slot, const int nDrive, const std::string & fileName, const bool createMissingDisk)
{
  std::string strPathName;

  if (fileName.empty())
  {
    return false;
  }

  if (fileName[0] == '/')
  {
    // Abs pathname
    strPathName = fileName;
  }
  else
  {
    // Rel pathname
    char szCWD[MAX_PATH] = {0};
    if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
      return false;

    strPathName = szCWD;
    strPathName.append("/");
    strPathName.append(fileName);
  }

  Disk2InterfaceCard* pDisk2Card = dynamic_cast<Disk2InterfaceCard*> (GetCardMgr().GetObj(slot));
  ImageError_e Error = pDisk2Card->InsertDisk(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
  return Error == eIMAGE_ERROR_NONE;
}

void setSnapshotFilename(const std::string & filename)
{
  // same logic as qapple
  // setting chdir allows to load relative disks from the snapshot file (tests?)
  // but if the snapshot file itself is relative, it wont work after a chdir
  // so we convert to absolute first
  char * absPath = realpath(filename.c_str(), nullptr);
  if (absPath)
  {
    char * temp = strdup(absPath);
    const char * dir = dirname(temp);
    // dir points inside temp!
    chdir(dir);
    Snapshot_SetFilename(absPath);

    free(temp);
    free(absPath);
    Snapshot_LoadState();
  }
}
