#include "StdAfx.h"

#include "Video.h"
#include "Common.h"
#include "CardManager.h"
#include "Applewin.h"
#include "Memory.h"
#include "Common.h"
#include "Frame.h"
#include "NTSC.h"
#include "Disk.h"
#include "CPU.h"

#include "linux/benchmark.h"

#include <chrono>

void VideoBenchmark(std::function<void()> redraw, std::function<void()> refresh)
{
  // PREPARE TWO DIFFERENT FRAME BUFFERS, EACH OF WHICH HAVE HALF OF THE
  // BYTES SET TO 0x14 AND THE OTHER HALF SET TO 0xAA
  int     loop;
  LPDWORD mem32 = (LPDWORD)mem;
  for (loop = 4096; loop < 6144; loop++)
    *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0x14141414
                                                        : 0xAAAAAAAA;
  for (loop = 6144; loop < 8192; loop++)
    *(mem32+loop) = ((loop & 1) ^ ((loop & 0x40) >> 6)) ? 0xAAAAAAAA
                                                        : 0x14141414;

  // SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totalhiresfps = 0;
  g_uVideoMode             = VF_HIRES;
  FillMemory(mem+0x2000,0x2000,0x14);
  redraw();

  auto start = std::chrono::steady_clock::now();
  long elapsed;

  do {
    if (totalhiresfps & 1)
      FillMemory(mem+0x2000,0x2000,0x14);
    else
      CopyMemory(mem+0x2000,mem+((totalhiresfps & 2) ? 0x4000 : 0x6000),0x2000);
    refresh();
    totalhiresfps++;

    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

  // DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
  // NOTHING ELSE GOING ON
  DWORD totalmhz10[2] = {0,0};	// bVideoUpdate & !bVideoUpdate
  for (UINT i=0; i<2; i++)
  {
    CpuSetupBenchmark();
    start = std::chrono::steady_clock::now();
    do {
      CpuExecute(100000, i==0 ? true : false);
      totalmhz10[i]++;
      const auto end = std::chrono::steady_clock::now();
      elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    } while (elapsed < 1000);
  }

  // IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
  // CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
  if ((regs.pc < 0x300) || (regs.pc > 0x400))
    if (MessageBox(g_hFrameWindow,
                   TEXT("The emulator has detected a problem while running ")
                   TEXT("the CPU benchmark.  Would you like to gather more ")
                   TEXT("information?"),
                   TEXT("Benchmarks"),
                   MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES) {
      BOOL error  = 0;
      WORD lastpc = 0x300;
      int  loop   = 0;
      while ((loop < 10000) && !error) {
        CpuSetupBenchmark();
        CpuExecute(loop, true);
        if ((regs.pc < 0x300) || (regs.pc > 0x400))
          error = 1;
        else {
          lastpc = regs.pc;
          ++loop;
        }
      }
      if (error) {
        TCHAR outstr[256];
        wsprintf(outstr,
                 TEXT("The emulator experienced an error %u clock cycles ")
                 TEXT("into the CPU benchmark.  Prior to the error, the ")
                 TEXT("program counter was at $%04X.  After the error, it ")
                 TEXT("had jumped to $%04X."),
                 (unsigned)loop,
                 (unsigned)lastpc,
                 (unsigned)regs.pc);
        MessageBox(g_hFrameWindow,
                   outstr,
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
      }
      else
        MessageBox(g_hFrameWindow,
                   TEXT("The emulator was unable to locate the exact ")
                   TEXT("point of the error.  This probably means that ")
                   TEXT("the problem is external to the emulator, ")
                   TEXT("happening asynchronously, such as a problem in ")
                   TEXT("a timer interrupt handler."),
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
    }

  // DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
  // WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
  // THE SAME TIME
  DWORD realisticfps = 0;
  FillMemory(mem+0x2000,0x2000,0xAA);
  redraw();

  start = std::chrono::steady_clock::now();

  const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
  const UINT cyclesPerMs = g_fCurrentCLK6502 / 1000;

  int cyclesThisFrame = 0;
  do {
    // this is a simplified version of AppleWin.cpp:ContinueExecution()
    const DWORD executedcycles = CpuExecute(cyclesPerMs, true);
    cyclesThisFrame += executedcycles;
    // every ms disk and joystick are updated
    GetCardMgr().GetDisk2CardMgr().UpdateDriveState(executedcycles);
#if 0
    JoyUpdateButtonLatch(executedcycles);
#endif
    if (cyclesThisFrame >= dwClksPerFrame)
    {
      cyclesThisFrame -= dwClksPerFrame;
      if (realisticfps & 1)
	FillMemory(mem+0x2000,0x2000,0xAA);
      else
	CopyMemory(mem+0x2000,mem+((realisticfps & 2) ? 0x4000 : 0x6000),0x2000);
      realisticfps++;
      refresh();
    }
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

  // DISPLAY THE RESULTS
  TCHAR outstr[256];
  wsprintf(outstr,
           TEXT("Pure Video FPS:\t%u\n")
           TEXT("Pure CPU MHz:\t%u.%u%s (video update)\n")
           TEXT("Pure CPU MHz:\t%u.%u%s (full-speed)\n\n")
           TEXT("EXPECTED AVERAGE VIDEO GAME\n")
           TEXT("PERFORMANCE: %u FPS"),
           (unsigned)totalhiresfps,
           (unsigned)(totalmhz10[0] / 10), (unsigned)(totalmhz10[0] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)(totalmhz10[1] / 10), (unsigned)(totalmhz10[1] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)realisticfps);
  MessageBox(g_hFrameWindow,
             outstr,
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
