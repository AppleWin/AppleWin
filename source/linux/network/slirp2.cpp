#include <StdAfx.h>

#include "linux/network/slirp2.h"

#ifdef U2_USE_SLIRP

#include <libslirp.h>

#include "Log.h"
#include "CPU.h"
#include "Core.h"

#define IP_PACK(a,b,c,d) htonl( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d))

namespace
{

  short vdeslirp_slirp_to_poll(int events)
  {
    short ret = 0;
    if (events & SLIRP_POLL_IN)  ret |= POLLIN;
    if (events & SLIRP_POLL_OUT) ret |= POLLOUT;
    if (events & SLIRP_POLL_PRI) ret |= POLLPRI;
    if (events & SLIRP_POLL_ERR) ret |= POLLERR;
    if (events & SLIRP_POLL_HUP) ret |= POLLHUP;
    return ret;
  }

  int vdeslirp_poll_to_slirp(short events)
  {
    int ret = 0;
    if (events & POLLIN)  ret |= SLIRP_POLL_IN;
    if (events & POLLOUT) ret |= SLIRP_POLL_OUT;
    if (events & POLLPRI) ret |= SLIRP_POLL_PRI;
    if (events & POLLERR) ret |= SLIRP_POLL_ERR;
    if (events & POLLHUP) ret |= SLIRP_POLL_HUP;
    return ret;
  }

  ssize_t net_slirp_send_packet(const void *buf, size_t len, void *opaque)
  {
    SlirpBackend * slirp = reinterpret_cast<SlirpBackend *>(opaque);
    slirp->sendToGuest(reinterpret_cast<const uint8_t *>(buf), len);
    return len;
  }

  void net_slirp_guest_error(const char *msg, void *opaque)
  {
    LogOutput("SLIRP: %s\n", msg);
  }

  int64_t net_slirp_clock_get_ns(void *opaque)
  {
    const int64_t ns = (10e9 * g_nCumulativeCycles) / g_fCurrentCLK6502;
    return ns;
  }

  void *net_slirp_timer_new(SlirpTimerCb cb, void *cb_opaque, void *opaque)
  {
    LogFileOutput("SLIRP: slirp_timer_new()\n");
    return nullptr;
  }

  void net_slirp_timer_free(void *timer, void *opaque)
  {
    LogFileOutput("SLIRP: slirp_timer_free()\n");
  }

  void net_slirp_timer_mod(void *timer, int64_t expire_timer, void *opaque)
  {
    LogFileOutput("SLIRP: slirp_timer_mod()\n");
  }

  void net_slirp_register_poll_fd(int /* fd */, void * /* opaque */)
  {
    // most existing implementations are a NOOP???
  }

  void net_slirp_unregister_poll_fd(int /* fd */, void * /* opaque */)
  {
    // most existing implementations are a NOOP???
  }

  void net_slirp_notify(void *opaque)
  {
    LogFileOutput("SLIRP: slirp_notify()\n");
  }

  int net_slirp_add_poll(int fd, int events, void *opaque)
  {
    SlirpBackend * slirp = reinterpret_cast<SlirpBackend *>(opaque);
    return slirp->addPoll(fd, events);
  }

  int net_slirp_get_revents(int idx, void *opaque)
  {
    const SlirpBackend * slirp = reinterpret_cast<SlirpBackend *>(opaque);
	  return slirp->getREvents(idx);
  }

}

SlirpBackend::SlirpBackend()
{
  const SlirpConfig cfg =
  {
    .version = SLIRP_CONFIG_VERSION_MAX,
    .in_enabled = 1,
    .vnetwork = { .s_addr = IP_PACK(10, 0, 0, 0) },
    .vnetmask = { .s_addr = IP_PACK(255, 255, 255, 0) },
    .vhost = { .s_addr = IP_PACK(10, 0, 0, 1) },
    .vhostname = "applewin",
    .vdhcp_start = { .s_addr = IP_PACK(10, 0, 0, 2) },
  };
  static const SlirpCb slirp_cb =
  {
    .send_packet = net_slirp_send_packet,
    .guest_error = net_slirp_guest_error,
    .clock_get_ns = net_slirp_clock_get_ns,
    .timer_new = net_slirp_timer_new,
    .timer_free = net_slirp_timer_free,
    .timer_mod = net_slirp_timer_mod,
    .register_poll_fd = net_slirp_register_poll_fd,
    .unregister_poll_fd = net_slirp_unregister_poll_fd,
    .notify = net_slirp_notify,
  };
  Slirp * slirp = slirp_new(&cfg, &slirp_cb, this);
  mySlirp.reset(slirp, slirp_cleanup);
}

void SlirpBackend::transmit(const int txlength,	uint8_t *txframe)
{
  slirp_input(mySlirp.get(), txframe, txlength);
}

int SlirpBackend::receive(const int size,	uint8_t * rxframe)
{
  if (!myQueue.empty())
  {
    const std::vector<uint8_t> & packet = myQueue.front();
    int received = std::min(static_cast<int>(packet.size()), size);
    memcpy(rxframe, packet.data(), received);
    if (received & 1)
    {
      rxframe[received] = 0;
      ++received;
    }
    myQueue.pop();
    return received;
  }
  else
  {
    return -1;
  }
}

void SlirpBackend::sendToGuest(const uint8_t *pkt, int pkt_len)
{
  if (myQueue.size() < ourQueueSize)
  {
    myQueue.emplace(pkt, pkt + pkt_len);
  }
  else
  {
    // drop it
  }
}

void SlirpBackend::update(const ULONG /* nExecutedCycles */)
{
  uint32_t timeout = 0;
  myFDs.clear();
  slirp_pollfds_fill(mySlirp.get(), &timeout, net_slirp_add_poll, this);
  int pollout;
  if (myFDs.empty())
  {
    pollout = 0;
  }
  else
  {
    pollout = poll(myFDs.data(), myFDs.size(), timeout);
  }
  slirp_pollfds_poll(mySlirp.get(), (pollout <= 0), net_slirp_get_revents, this);
}

int SlirpBackend::addPoll(const int fd, const int events)
{
  const pollfd ff = { .fd = fd, .events = vdeslirp_slirp_to_poll(events) };
  myFDs.push_back(ff);
  return myFDs.size() - 1;
}

int SlirpBackend::getREvents(const int idx) const
{
  return vdeslirp_poll_to_slirp(myFDs[idx].revents);
}

bool SlirpBackend::isValid()
{
  return true;
}

void SlirpBackend::getMACAddress(const uint32_t address, MACAddress & mac)
{
  // a bit of a hack, I've found out that slirp uses
  /* emulated hosts use the MAC addr 52:55:IP:IP:IP:IP */
  // https://gitlab.freedesktop.org/slirp/libslirp/-/blob/bf917b89d64f57d9302aba4b2f027ea68fb78c13/src/slirp.c#L78
  mac.address[0] = 0x52;
  mac.address[1] = 0x55;
  uint32_t * ptr = reinterpret_cast<uint32_t *>(mac.address + 2);
  *ptr = address;
}

const std::string & SlirpBackend::getInterfaceName()
{
  return myEmptyInterface;
}

#endif
