#pragma once

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

class SlirpNet
{
public:
  SlirpNet();
  void sendFromGuest(const uint8_t *pkt, int pkt_len);
  void sendToGuest(const uint8_t *pkt, int pkt_len);

  void process(const uint32_t timeout);
  int addPoll(const int fd, const int events);
  int getREvents(const int idx) const;

  std::queue<std::vector<uint8_t>> & getQueue();
  void clearQueue();
private:

  static constexpr size_t ourQueueSize = 10;

  std::shared_ptr<Slirp> mySlirp;
  std::vector<pollfd> myFDs;

  std::queue<std::vector<uint8_t>> myQueue;
};

#endif
