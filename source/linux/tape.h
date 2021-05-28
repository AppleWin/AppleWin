#pragma once

#include <vector>
#include <cstdint>

typedef int8_t tape_data_t;

void setCassetteTape(const std::vector<tape_data_t> & data, const int frequency);
void getTapeInfo(size_t & size, size_t & pos, int & frequency, uint8_t & bit);
void ejectTape();
