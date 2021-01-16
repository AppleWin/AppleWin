#include "frontends/common2/utils.h"

#include "linux/interface.h"

#include "StdAfx.h"
#include "SaveState.h"

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Disk.h"
#include "Mockingboard.h"
#include "SoundCore.h"
#include "Harddisk.h"
#include "Speaker.h"
#include "Log.h"
#include "CPU.h"
#include "Memory.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Video.h"
#include "NTSC.h"
#include "SaveState.h"
#include "RGBMonitor.h"
#include "Riff.h"
#include "Utilities.h"
#include "Interface.h"

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

void initialiseEmulator()
{
#ifdef RIFF_SPKR
  RiffInitWriteFile("/tmp/Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
  RiffInitWriteFile("/tmp/Mockingboard.wav", 44100, 2);
#endif

  g_nAppMode = MODE_RUNNING;
  LogFileOutput("Initialisation\n");

  g_bFullSpeed = false;

  LoadConfiguration();
  SetCurrentCLK6502();
  GetAppleWindowTitle();
  GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);

  DSInit();
  MB_Initialize();
  SpkrInitialize();

  MemInitialize();
  GetFrame().Initialize();

  GetCardMgr().GetDisk2CardMgr().Reset();
  HD_Reset();
  Snapshot_Startup();
}

void uninitialiseEmulator()
{
  Snapshot_Shutdown();
  CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
  if (pMouseCard)
  {
    pMouseCard->Reset();
  }
  MemDestroy();
  GetFrame().Destroy();

  SpkrDestroy();
  MB_Destroy();
  DSUninit();

  HD_Destroy();
  PrintDestroy();
  CpuDestroy();

  GetCardMgr().GetDisk2CardMgr().Destroy();
  LogDone();
  RiffFinishWriteFile();
}
