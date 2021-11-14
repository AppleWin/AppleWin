#include "StdAfx.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"

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
#include "Registry.h"
#include "Utilities.h"
#include "Interface.h"
#include "Debugger/Debug.h"
#include "Tfe/tfe.h"

#include <libgen.h>
#include <unistd.h>

namespace common2
{

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

  void InitialiseEmulator()
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

    CardManager & cardManager = GetCardMgr();
    cardManager.GetDisk2CardMgr().Reset();
    if (cardManager.QuerySlot(SLOT7) == CT_GenericHDD)
    {
        dynamic_cast<HarddiskInterfaceCard&>(cardManager.GetRef(SLOT7)).Reset(true);
    }

    Snapshot_Startup();

    DebugInitialize();
  }

  void DestroyEmulator()
  {
    CardManager & cardManager = GetCardMgr();

    Snapshot_Shutdown();
    CMouseInterface* pMouseCard = cardManager.GetMouseCard();
    if (pMouseCard)
    {
      pMouseCard->Reset();
    }
    MemDestroy();

    SpkrDestroy();
    MB_Destroy();
    DSUninit();

    tfe_shutdown();

    if (cardManager.QuerySlot(SLOT7) == CT_GenericHDD)
    {
        dynamic_cast<HarddiskInterfaceCard&>(cardManager.GetRef(SLOT7)).Destroy();
    }

    PrintDestroy();
    CpuDestroy();
    DebugDestroy();

    GetCardMgr().GetDisk2CardMgr().Destroy();
    RiffFinishWriteFile();
  }

  void loadGeometryFromRegistry(const std::string &section, Geometry & geometry)
  {
    if (geometry.empty)  // otherwise it was user provided
    {
      const std::string path = section + "\\geometry";
      const auto loadValue = [&path](const char * name, int & dest)
      {
        DWORD value;
        if (RegLoadValue(path.c_str(), name, TRUE, &value))
        {
          // DWORD and int have the same size
          // but if they did not, this would be necessary
          typedef std::make_signed<DWORD>::type signed_t;
          dest = static_cast<signed_t>(value);
        }
      };

      loadValue("width", geometry.width);
      loadValue("height", geometry.height);
      loadValue("x", geometry.x);
      loadValue("y", geometry.y);
    }
  }

  void saveGeometryToRegistry(const std::string &section, const Geometry & geometry)
  {
    const std::string path = section + "\\geometry";
    const auto saveValue = [&path](const char * name, const int source)
    {
      // this seems to already do the right thing for negative numbers
      RegSaveValue(path.c_str(), name, TRUE, source);
    };
    saveValue("width", geometry.width);
    saveValue("height", geometry.height);
    saveValue("x", geometry.x);
    saveValue("y", geometry.y);
  }

}
