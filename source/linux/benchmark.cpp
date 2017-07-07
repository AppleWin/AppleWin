#include "StdAfx.h"

#include <chrono>

#include "Video.h"
#include "Memory.h"
#include "Common.h"
#include "Frame.h"
#include "Disk.h"
#include "CPU.h"

void VideoBenchmark(std::function<void()> VideoRedrawScreen)
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

  // SEE HOW MANY TEXT FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totaltextfps = 0;

  g_uVideoMode            = VF_TEXT;
  FillMemory(mem+0x400,0x400,0x14);
  VideoRedrawScreen();

  auto start = std::chrono::steady_clock::now();
  long elapsed;

  DWORD cycle = 0;
  do {
    if (cycle & 1)
      FillMemory(mem+0x400,0x400,0x14);
    else
      CopyMemory(mem+0x400,mem+((cycle & 2) ? 0x4000 : 0x6000),0x400);
    VideoRedrawScreen();
    if (cycle++ >= 3)
      cycle = 0;
    totaltextfps++;
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

  // SEE HOW MANY HIRES FRAMES PER SECOND WE CAN PRODUCE WITH NOTHING ELSE
  // GOING ON, CHANGING HALF OF THE BYTES IN THE VIDEO BUFFER EACH FRAME TO
  // SIMULATE THE ACTIVITY OF AN AVERAGE GAME
  DWORD totalhiresfps = 0;
  g_uVideoMode             = VF_HIRES;
  FillMemory(mem+0x2000,0x2000,0x14);
  VideoRedrawScreen();

  start = std::chrono::steady_clock::now();

  cycle = 0;
  do {
    if (cycle & 1)
      FillMemory(mem+0x2000,0x2000,0x14);
    else
      CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoRedrawScreen();
    if (cycle++ >= 3)
      cycle = 0;
    totalhiresfps++;
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

  // DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
  // NOTHING ELSE GOING ON
  CpuSetupBenchmark();
  DWORD totalmhz10 = 0;

  start = std::chrono::steady_clock::now();

  cycle = 0;
  do {
    CpuExecute(100000, true);
    totalmhz10++;
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

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
  VideoRedrawScreen();
  start = std::chrono::steady_clock::now();

  cycle = 0;
  do {
    if (realisticfps < 10) {
      int cycles = 100000;
      while (cycles > 0) {
        DWORD executedcycles = CpuExecute(103, true);
        cycles -= executedcycles;
        DiskUpdatePosition(executedcycles);
#if 0
        JoyUpdateButtonLatch(executedcycles);
#endif
      }
    }
    if (cycle & 1)
      FillMemory(mem+0x2000,0x2000,0xAA);
    else
      CopyMemory(mem+0x2000,mem+((cycle & 2) ? 0x4000 : 0x6000),0x2000);
    VideoRedrawScreen();
    if (cycle++ >= 3)
      cycle = 0;
    realisticfps++;
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  } while (elapsed < 1000);

  // DISPLAY THE RESULTS
  TCHAR outstr[256];
  wsprintf(outstr,
           TEXT("Pure Video FPS:\t%u hires, %u text\n")
           TEXT("Pure CPU MHz:\t%u.%u%s\n\n")
           TEXT("EXPECTED AVERAGE VIDEO GAME")
           TEXT("PERFORMANCE: %u FPS"),
           (unsigned)totalhiresfps,
           (unsigned)totaltextfps,
           (unsigned)(totalmhz10/10),
           (unsigned)(totalmhz10 % 10),
           (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
           (unsigned)realisticfps);
  MessageBox(g_hFrameWindow,
             outstr,
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
