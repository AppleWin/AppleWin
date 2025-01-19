#pragma once

#ifndef _WIN32
#include <arpa/inet.h>
#else
#include "Tfe/pcap.h"
#endif

#include <vector>
#include <string>

struct PortFwd
{
  int slot;
  int is_udp;
  in_addr host_addr;
  int host_port;
  in_addr guest_addr;
  int guest_port;

  std::string toString() const;
};

std::vector<PortFwd> getPortFwds(const std::vector<std::string> & specs);
