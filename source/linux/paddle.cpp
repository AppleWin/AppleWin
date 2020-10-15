#include "StdAfx.h"

#include "linux/paddle.h"

#include "Log.h"
#include "Memory.h"
#include "Common.h"
#include "CPU.h"

namespace
{
  unsigned __int64 g_nJoyCntrResetCycle = 0;	// Abs cycle that joystick counters were reset
  const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;	// 11.04 (From KEGS)
}



std::shared_ptr<const Paddle> & Paddle::instance()
{
  static std::shared_ptr<const Paddle> singleton = std::make_shared<Paddle>();
  return singleton;
}

std::set<int> Paddle::ourButtons;

void Paddle::setButtonPressed(int i)
{
  ourButtons.insert(i);
}

void Paddle::setButtonReleased(int i)
{
  ourButtons.erase(i);
}




Paddle::Paddle()
{
}

bool Paddle::getButton(int i) const
{
  return false;
}

double Paddle::getAxis(int i) const
{
  return 0;
}

int Paddle::getAxisValue(int i) const
{
  const double axis = getAxis(i);
  const int value = static_cast<int>((axis + 1.0) * 0.5 * 255);
  return value;
}

Paddle::~Paddle()
{
}

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
  addr &= 0xFF;
  BOOL pressed = 0;

  if (Paddle::ourButtons.find(addr) != Paddle::ourButtons.end())
  {
    pressed = 1;
  }
  else
  {
    const std::shared_ptr<const Paddle> & paddle = Paddle::instance();

    if (paddle)
    {
      switch (addr)
      {
      case Paddle::ourOpenApple:
        pressed = paddle->getButton(0);
        break;
      case Paddle::ourSolidApple:
        pressed = paddle->getButton(1);
        break;
      case Paddle::ourThirdApple:
        break;
      }
    }
  }

  return MemReadFloatingBus(pressed, uExecutedCycles);
}

BYTE __stdcall JoyReadPosition(WORD pc, WORD address, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
  const int nJoyNum = (address & 2) ? 1 : 0;	// $C064..$C067

  CpuCalcCycles(uExecutedCycles);
  BOOL nPdlCntrActive = 0;

  const std::shared_ptr<const Paddle> & paddle = Paddle::instance();

  if (paddle)
  {
    if (nJoyNum == 0)
    {
      int axis = address & 1;
      int pdl = paddle->getAxisValue(axis);
      // This is from KEGS. It helps games like Championship Lode Runner & Boulderdash
      if (pdl >= 255)
	pdl = 280;

      nPdlCntrActive  = g_nCumulativeCycles <= (g_nJoyCntrResetCycle + (unsigned __int64) ((double)pdl * PDL_CNTR_INTERVAL));
    }
  }

  return MemReadFloatingBus(nPdlCntrActive, uExecutedCycles);
}

void JoyResetPosition(ULONG uExecutedCycles)
{
  CpuCalcCycles(uExecutedCycles);
  g_nJoyCntrResetCycle = g_nCumulativeCycles;
}
