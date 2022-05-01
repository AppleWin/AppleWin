#include "StdAfx.h"

#include "Video.h"
#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Memory.h"
#include "Common.h"
#include "NTSC.h"
#include "Disk.h"
#include "CPU.h"
#include "Interface.h"

#include "linux/benchmark.h"

#include <chrono>

void VideoBenchmark(std::function<void()> redraw, std::function<void()> refresh)
{
  FrameBase & frame = GetFrame();
  Video & video = GetVideo();
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
  video.SetVideoMode(VF_HIRES);
  memset(mem+0x2000,0x14,0x2000);
  redraw();

  typedef std::chrono::microseconds interval_t;
  typedef uint64_t counter_t; // avoid overflows
  const counter_t onesecond = 1000000;
  counter_t totalhiresfps = 0;
  counter_t elapsed;

  auto start = std::chrono::steady_clock::now();
  do {
    if (totalhiresfps & 1)
      memset(mem+0x2000,0x14,0x2000);
    else
      memcpy(mem+0x2000,mem+((totalhiresfps & 2) ? 0x4000 : 0x6000),0x2000);
    refresh();
    totalhiresfps++;

    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<interval_t>(end - start).count();
  } while (elapsed < onesecond);
  // adjust for broken ms
  totalhiresfps = totalhiresfps * onesecond / elapsed;

  // DETERMINE HOW MANY 65C02 CLOCK CYCLES WE CAN EMULATE PER SECOND WITH
  // NOTHING ELSE GOING ON
  counter_t totalmhz10[2] = {0, 0};	// bVideoUpdate & !bVideoUpdate
  for (size_t i = 0; i < 2; i++)
  {
    CpuSetupBenchmark();
    start = std::chrono::steady_clock::now();
    do {
      CpuExecute(100000, i==0 ? true : false);
      totalmhz10[i]++;
      const auto end = std::chrono::steady_clock::now();
      elapsed = std::chrono::duration_cast<interval_t>(end - start).count();
    } while (elapsed < onesecond);
    totalmhz10[i] = totalmhz10[i] * onesecond / elapsed;
  }

  // IF THE PROGRAM COUNTER IS NOT IN THE EXPECTED RANGE AT THE END OF THE
  // CPU BENCHMARK, REPORT AN ERROR AND OPTIONALLY TRACK IT DOWN
  if ((regs.pc < 0x300) || (regs.pc > 0x400))
    if (frame.FrameMessageBox(
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
        const std::string outstr = StrFormat(
          TEXT("The emulator experienced an error %u clock cycles ")
          TEXT("into the CPU benchmark.  Prior to the error, the ")
          TEXT("program counter was at $%04X.  After the error, it ")
          TEXT("had jumped to $%04X."),
          (unsigned)loop,
          (unsigned)lastpc,
          (unsigned)regs.pc);
        frame.FrameMessageBox(
                   outstr.c_str(),
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
      }
      else
      {
        frame.FrameMessageBox(
                   TEXT("The emulator was unable to locate the exact ")
                   TEXT("point of the error.  This probably means that ")
                   TEXT("the problem is external to the emulator, ")
                   TEXT("happening asynchronously, such as a problem in ")
                   TEXT("a timer interrupt handler."),
                   TEXT("Benchmarks"),
                   MB_ICONINFORMATION | MB_SETFOREGROUND);
      }
    }

  // DO A REALISTIC TEST OF HOW MANY FRAMES PER SECOND WE CAN PRODUCE
  // WITH FULL EMULATION OF THE CPU, JOYSTICK, AND DISK HAPPENING AT
  // THE SAME TIME
  counter_t realisticfps = 0;
  memset(mem+0x2000,0xAA,0x2000);
  redraw();

  const size_t dwClksPerFrame = NTSC_GetCyclesPerFrame();
  const size_t cyclesPerMs = g_fCurrentCLK6502 / 1000;

  counter_t cyclesThisFrame = 0;
  start = std::chrono::steady_clock::now();

  do {
    // this is a simplified version of AppleWin.cpp:ContinueExecution()
    const DWORD executedcycles = CpuExecute(cyclesPerMs, true);
    cyclesThisFrame += executedcycles;
    // every ms disk and joystick are updated
    GetCardMgr().GetDisk2CardMgr().Update(executedcycles);
#if 0
    JoyUpdateButtonLatch(executedcycles);
#endif
    if (cyclesThisFrame >= dwClksPerFrame)
    {
      cyclesThisFrame -= dwClksPerFrame;
      if (realisticfps & 1)
	memset(mem+0x2000,0xAA,0x2000);
      else
	memcpy(mem+0x2000,mem+((realisticfps & 2) ? 0x4000 : 0x6000),0x2000);
      realisticfps++;
      refresh();
    }
    const auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<interval_t>(end - start).count();
  } while (elapsed < onesecond);
  realisticfps = realisticfps * onesecond / elapsed;

  // DISPLAY THE RESULTS
  const std::string outstr = StrFormat(
    TEXT("Pure Video FPS:\t%u\n")
    TEXT("Pure CPU MHz:\t%u.%u%s (video update)\n")
    TEXT("Pure CPU MHz:\t%u.%u%s (full-speed)\n\n")
    TEXT("EXPECTED AVERAGE VIDEO GAME\n")
    TEXT("PERFORMANCE: %u FPS"),
    (unsigned)totalhiresfps,
    (unsigned)(totalmhz10[0] / 10), (unsigned)(totalmhz10[0] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
    (unsigned)(totalmhz10[1] / 10), (unsigned)(totalmhz10[1] % 10), (LPCTSTR)(IS_APPLE2 ? TEXT(" (6502)") : TEXT("")),
    (unsigned)realisticfps);
  frame.FrameMessageBox(
             outstr.c_str(),
             TEXT("Benchmarks"),
             MB_ICONINFORMATION | MB_SETFOREGROUND);
}
