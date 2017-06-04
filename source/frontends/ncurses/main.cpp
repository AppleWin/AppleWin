#include "StdAfx.h"

#include <chrono>
#include <iostream>
#include <unistd.h>

#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "Video.h"

#include "frontends/ncurses/world.h"
#include "ncurses.h"

static bool ContinueExecution()
{
  const auto start = std::chrono::steady_clock::now();

  const double fUsecPerSec        = 1.e6;
#if 1
  const UINT nExecutionPeriodUsec = 1000000 / 60;		// 60 FPS
  //	const UINT nExecutionPeriodUsec = 100;		// 0.1ms
  const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
#else
  const double fExecutionPeriodClks = 1800.0;
  const UINT nExecutionPeriodUsec = (UINT) (fUsecPerSec * (fExecutionPeriodClks / g_fCurrentCLK6502));
#endif

  const DWORD uCyclesToExecute = fExecutionPeriodClks;

  const bool bVideoUpdate = false;
  const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
  g_dwCyclesThisFrame += uActualCyclesExecuted;

  DiskUpdatePosition(uActualCyclesExecuted);

  const int key = ProcessKeyboard();

  switch (key)
  {
  case KEY_F(2):
    return false;
  }

  if (g_dwCyclesThisFrame >= dwClksPerFrame)
  {
    g_dwCyclesThisFrame -= dwClksPerFrame;
    VideoRedrawScreen();
  }

  if (!DiskIsSpinning())
  {
    const auto end = std::chrono::steady_clock::now();
    const auto diff = end - start;
    const long ms = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();

    if (ms < nExecutionPeriodUsec)
    {
      usleep(nExecutionPeriodUsec - ms);
    }
  }

  return true;
}

void EnterMessageLoop()
{
  while (ContinueExecution())
  {
  }
}

static int DoDiskInsert(const int nDrive, LPCSTR szFileName)
{
  std::string strPathName;

  if (szFileName[0] == '/')
  {
    // Abs pathname
    strPathName = szFileName;
  }
  else
  {
    // Rel pathname
    char szCWD[MAX_PATH] = {0};
    if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
      return false;

    strPathName = szCWD;
    strPathName.append("/");
    strPathName.append(szFileName);
  }

  ImageError_e Error = DiskInsert(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
  return Error == eIMAGE_ERROR_NONE;
}

void foo(int argc, const char * argv [])
{
  bool bShutdown = false;
  bool bSetFullScreen = false;
  bool bBoot = false;

  g_fh = fopen("/tmp/applewin.txt", "w");
  setbuf(g_fh, NULL);

  LogFileOutput("Initialisation\n");

  ImageInitialize();
  DiskInitialize();
  int nError = 0;	// TODO: Show error MsgBox if we get a DiskInsert error
  if (argc >= 2)
  {
    nError = DoDiskInsert(DRIVE_1, argv[1]);
    LogFileOutput("Init: DoDiskInsert(D1), res=%d\n", nError);
    FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
    bBoot = true;
  }

  if (argc >= 3)
  {
    nError |= DoDiskInsert(DRIVE_2, argv[2]);
    LogFileOutput("Init: DoDiskInsert(D2), res=%d\n", nError);
  }

  do
  {
    MemInitialize();
    VideoInitialize();
    DiskReset();
    EnterMessageLoop();
  }
  while (g_bRestart);

  VideoUninitialize();

}

int main(int argc, const char * argv [])
{
  foo(argc, argv);
  return 0;
}
