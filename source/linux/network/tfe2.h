#pragma once

bool tfeReceiveOnePacket(const uint8_t * mac, BYTE * buffer, int & len);
void tfeTransmitOnePacket(const BYTE * buffer, const int len);
void tfeTransmitOneUDPPacket(const uint8_t * dest, const BYTE * buffer, const int len);
uint16_t readNetworkWord(const uint8_t * address);
