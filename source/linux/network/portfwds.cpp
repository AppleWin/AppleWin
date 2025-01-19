#include "linux/network/portfwds.h"

#ifndef _WIN32

#include "StrFormat.h"

#include <cstring>
#include <regex>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

namespace
{

  void parseAddress(const addrinfo & hints, const std::string & addr, const std::string & port,
                    in_addr & host_addr, int & host_port)
  {
    addrinfo *result = nullptr;
    const char * a = addr.empty() ? nullptr : addr.c_str();
    const int r = getaddrinfo(a, port.c_str(), &hints, &result);
    if (r)
    {
      throw std::runtime_error(StrFormat("getaddrinfo('%s:%s') [%s]", addr.c_str(), port.c_str(), gai_strerror(r)));
    }

    const addrinfo *res = result;
    while (res)
    {
      if (res->ai_family == AF_INET)
      {
        const sockaddr_in * ip = reinterpret_cast<const sockaddr_in *>(res->ai_addr);
        host_addr = ip->sin_addr;
        host_port = ntohs(ip->sin_port);
        freeaddrinfo(result); // we should find a better RAII
        return;
      }
      res = res->ai_next;
    }

    freeaddrinfo(result);
    throw std::runtime_error(StrFormat("No valid address: '%s:%s'", addr.c_str(), port.c_str()));
  }

}

std::string PortFwd::toString() const
{
  char address1[INET_ADDRSTRLEN + 1];
  char address2[INET_ADDRSTRLEN + 1];
  const char * h = inet_ntop(AF_INET, &host_addr, address1, sizeof(address1));
  const char * g = inet_ntop(AF_INET, &guest_addr, address2, sizeof(address2));
  const std::string str = StrFormat("%s:%d -> %s:%d", h, host_port, g, guest_port);
  return str;
}

std::vector<PortFwd> getPortFwds(const std::vector<std::string> & specs)
{
  std::vector<PortFwd> fwds;
  addrinfo hints;
  memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = 0;
  hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

  const std::regex re("^(\\d),(udp|tcp),([a-zA-Z0-9.]*?),(\\w+?),([a-zA-Z0-9.]*?),(\\w+?)$");

  for (const auto & value : specs)
  {
    std::smatch m;
    if (std::regex_match(value, m, re) && m.size() == 7)
    {
      PortFwd fwd;
      fwd.slot = std::stoi(m.str(1));
      fwd.is_udp = m.str(2) == "udp";
      parseAddress(hints, m.str(3), m.str(4), fwd.host_addr, fwd.host_port);
      parseAddress(hints, m.str(5), m.str(6), fwd.guest_addr, fwd.guest_port);
      fwds.push_back(std::move(fwd));
    }
    else
    {
      throw std::runtime_error(StrFormat("Invalid NAT spec: '%s'", value.c_str()));
    }
  }
  return fwds;
}

#else

std::string PortFwd::toString() const
{
  return "";
}

std::vector<PortFwd> getPortFwds(const std::vector<std::string> & specs)
{
  return {};
}
  
#endif
