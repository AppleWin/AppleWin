#include "StdAfx.h"

#include "linux/tape.h"

#include "Core.h"
#include "Tape.h"
#include "Memory.h"
#include "Pravets.h"
#include "CPU.h"

#include <limits>

namespace
{
  std::vector<tape_data_t> cassetteTape;
  uint64_t baseCycles;
  int tapeFrequency;
  bool playing = false;
  BYTE lastBit = 1;
  // threshold not needed for https://asciiexpress.net
  // but probably necessary for a real audio file
  constexpr tape_data_t threshold = 5;

  BYTE getBitValue(const tape_data_t val)
  {
    // this has been tested on all formats from https://asciiexpress.net
    //
    // we are extracting the sign bit (set for negative numbers)
    // this is really important for the asymmetric wave in https://asciiexpress.net/diskserver/
    // not so much for the other cases
    if (val > threshold)
    {
      lastBit = 0;
    }
    else if (val < -threshold)
    {
      lastBit = 1;
    }
    // else leave it unchanged to the previous value
    return lastBit;
  }

  tape_data_t getCurrentWave(size_t & pos)
  {
    if (playing && !cassetteTape.empty())
    {
      const double delta = g_nCumulativeCycles - baseCycles;
      const double position = delta / g_fCurrentCLK6502 * tapeFrequency;
      pos = static_cast<size_t>(position);

      if (pos < cassetteTape.size() - 1)
      {
        // linear interpolation
        const double reminder = position - pos;
        const double value = (1.0 - reminder) * cassetteTape[pos] + reminder * cassetteTape[pos + 1];
        return lround(value);
      }

      cassetteTape.clear();
    }
    pos = 0;
    return std::numeric_limits<tape_data_t>::min();
  }
}

//---------------------------------------------------------------------------

BYTE __stdcall TapeRead(WORD pc, WORD address, BYTE, BYTE, ULONG nExecutedCycles)	// $C060 TAPEIN
{
  if (g_Apple2Type == A2TYPE_PRAVETS8A)
    return GetPravets().GetKeycode( MemReadFloatingBus(nExecutedCycles) );

  CpuCalcCycles(nExecutedCycles);

  if (!playing)
  {
    // start play as soon as TAPEIN is read
    playing = true;
    baseCycles = g_nCumulativeCycles;
  }

  size_t pos;
  const tape_data_t val = getCurrentWave(pos);  // this has side effects
  const BYTE highBit = getBitValue(val);

  return MemReadFloatingBus(highBit, nExecutedCycles);
}

BYTE __stdcall TapeWrite(WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles)	// $C020 TAPEOUT
{
  return 0;
}

void setCassetteTape(const std::vector<tape_data_t> & data, const int frequency)
{
  playing = false;
  cassetteTape = data;
  tapeFrequency = frequency;
}

void getTapeInfo(size_t & size, size_t & pos, int & frequency, uint8_t & bit)
{
  const tape_data_t val = getCurrentWave(pos);  // this has side effects
  bit = lastBit;
  size = cassetteTape.size();
  frequency = tapeFrequency;
}

void ejectTape()
{
  cassetteTape.clear();
}
