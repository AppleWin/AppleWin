#pragma once

struct MACAddress;

std::vector<uint8_t> createETH2Frame(const std::vector<uint8_t> &data,
                                     const MACAddress *sourceMac, const MACAddress *destinationMac,
                                     const uint8_t ttl, const uint8_t tos, const uint8_t protocol,
                                     const uint32_t sourceAddress, const uint32_t destinationAddress);

void getIPPayload(const int lengthOfFrame, const uint8_t *frame,
                  size_t &lengthOfPayload, const uint8_t *&payload, uint32_t &source, uint8_t &protocol);
