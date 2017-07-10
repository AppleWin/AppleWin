#include "StdAfx.h"

#include "frontends/ncurses/input.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include <libevdev/libevdev.h>

#include "Log.h"
#include "Memory.h"
#include "Common.h"
#include "CPU.h"

unsigned __int64 g_nJoyCntrResetCycle = 0;	// Abs cycle that joystick counters were reset
const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;	// 11.04 (From KEGS)

std::shared_ptr<Input> Input::ourSingleton;

void Input::initialise(const std::string & device)
{
  ourSingleton.reset(new Input(device));
}

Input & Input::instance()
{
  return *ourSingleton;
}

Input::Input(const std::string & device)
  : myButtonCodes(2), myAxisCodes(2), myAxisMins(2), myAxisMaxs(2)
{
  myFD = open(device.c_str(), O_RDONLY | O_NONBLOCK);
  if (myFD > 0)
  {
    libevdev * dev;
    int rc = libevdev_new_from_fd(myFD, &dev);
    if (rc < 0)
    {
      LogFileOutput("Input: failed to init libevdev (%s): %s\n", strerror(-rc), device.c_str());
    }
    else
    {
      myDev.reset(dev, libevdev_free);
      myButtonCodes[0] = BTN_SOUTH;
      myButtonCodes[1] = BTN_EAST;
      myAxisCodes[0] = ABS_X;
      myAxisCodes[1] = ABS_Y;

      for (size_t i = 0; i < myAxisCodes.size(); ++i)
      {
	myAxisMins[i] = libevdev_get_abs_minimum(myDev.get(), myAxisCodes[i]);
	myAxisMaxs[i] = libevdev_get_abs_maximum(myDev.get(), myAxisCodes[i]);
      }
    }
  }
  else
  {
    LogFileOutput("Input: failed to open device (%s): %s\n", strerror(errno), device.c_str());
  }
}

Input::~Input()
{
  if (myFD > 0)
  {
    close(myFD);
  }
}

int Input::poll()
{
  int counter = 0;
  if (!myDev)
  {
    return counter;
  }

  input_event ev;
  int rc = LIBEVDEV_READ_STATUS_SUCCESS;
  do
  {
    if (rc == LIBEVDEV_READ_STATUS_SYNC)
      rc = libevdev_next_event(myDev.get(), LIBEVDEV_READ_FLAG_SYNC, &ev);
    else
      rc = libevdev_next_event(myDev.get(), LIBEVDEV_READ_FLAG_NORMAL, &ev);
    ++counter;
  } while (rc >= 0);

  return counter;
}

bool Input::getButton(int i) const
{
  int value = 0;
  if (myDev)
  {
    int rc = libevdev_fetch_event_value(myDev.get(), EV_KEY, myButtonCodes[i], &value);
  }
  return value != 0;
}

int Input::getAxis(int i) const
{
  if  (myDev)
  {
    int value = 0;
    int rc = libevdev_fetch_event_value(myDev.get(), EV_ABS, myAxisCodes[i], &value);
    int pdl = 255 * (value - myAxisMins[i]) / (myAxisMaxs[i] - myAxisMins[i]);
    return pdl;
  }
  else
  {
    return 0;
  }

}

BYTE __stdcall JoyReadButton(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  addr &= 0xFF;
  BOOL pressed = 0;

  switch (addr)
  {
    case 0x61:
      pressed = Input::instance().getButton(0);
      break;
    case 0x62:
      pressed = Input::instance().getButton(1);
      break;
    case 0x63:
      break;
  }
  return MemReadFloatingBus(pressed, nCyclesLeft);
}

BYTE __stdcall JoyReadPosition(WORD pc, WORD address, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  const int nJoyNum = (address & 2) ? 1 : 0;	// $C064..$C067

  CpuCalcCycles(nCyclesLeft);
  BOOL nPdlCntrActive = 0;

  if (nJoyNum == 0)
  {
    int axis = address & 1;
    int pdl = Input::instance().getAxis(axis);
    // This is from KEGS. It helps games like Championship Lode Runner & Boulderdash
    if (pdl >= 255)
      pdl = 280;

    nPdlCntrActive  = g_nCumulativeCycles <= (g_nJoyCntrResetCycle + (unsigned __int64) ((double)pdl * PDL_CNTR_INTERVAL));
  }

  return MemReadFloatingBus(nPdlCntrActive, nCyclesLeft);
}

BYTE __stdcall JoyResetPosition(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
  CpuCalcCycles(nCyclesLeft);
  g_nJoyCntrResetCycle = g_nCumulativeCycles;

  return MemReadFloatingBus(nCyclesLeft);
}
