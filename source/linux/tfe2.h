#pragma once

#include <iosfwd>

bool tfeReceiveOnePacket(const uint8_t * mac, BYTE * buffer, int & len);
void tfeTransmitOnePacket(const BYTE * buffer, const int len);

std::ostream & as_hex(std::ostream & s, const size_t value, const size_t width);
std::ostream & stream_mac(std::ostream & s, const uint8_t * mac);
