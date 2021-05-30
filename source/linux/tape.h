#pragma once

#include <vector>
#include <cstdint>

class CassetteTape
{
public:

  typedef int8_t tape_data_t;

  void setData(const std::vector<tape_data_t> & data, const int frequency);
  BYTE getValue(const ULONG nExecutedCycles);

  void getTapeInfo(size_t & size, size_t & pos, int & frequency, uint8_t & bit) const ;
  void eject();
  void rewind();

  static CassetteTape & instance();

private:
  BYTE getBitValue(const tape_data_t val);
  tape_data_t getCurrentWave(size_t & pos) const;

  std::vector<tape_data_t> myData;

  uint64_t myBaseCycles;
  int myFrequency;
  bool myIsPlaying = false;
  BYTE myLastBit = 1; // negative wave

  static constexpr tape_data_t myThreshold = 5;
};
