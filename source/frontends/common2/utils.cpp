#include <frontends/common2/utils.h>

#include "StdAfx.h"
#include "SaveState.h"

#include <libgen.h>
#include <unistd.h>

void setSnapshotFilename(const std::string & filename, const bool load)
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

    if (load)
    {
      Snapshot_LoadState();
    }
  }
}
