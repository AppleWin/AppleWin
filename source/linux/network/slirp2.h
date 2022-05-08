#pragma once

#include "Tfe/NetworkBackend.h"

#include "linux/config.h"

#ifdef SLIRP_FOUND
// disable to use libpcap in all cases
#define U2_USE_SLIRP
#endif

#ifdef U2_USE_SLIRP

#include <memory>
#include <vector>
#include <queue>

#include <poll.h>

struct Slirp;

class SlirpBackend : public NetworkBackend
{
public:
  SlirpBackend();

  void transmit(const int txlength,	uint8_t *txframe) override;
  int receive(const int size,	uint8_t * rxframe) override;

  void update(const ULONG nExecutedCycles) override;
  bool isValid() override;

  void getMACAddress(const uint32_t address, MACAddress & mac) override;

  const std::string & getInterfaceName() override;

  void sendToGuest(const uint8_t *pkt, int pkt_len);

  int addPoll(const int fd, const int events);
  int getREvents(const int idx) const;
private:

  static constexpr size_t ourQueueSize = 10;

  const std::string myEmptyInterface;
  std::shared_ptr<Slirp> mySlirp;
  std::vector<pollfd> myFDs;

  std::queue<std::vector<uint8_t>> myQueue;
};

#endif
