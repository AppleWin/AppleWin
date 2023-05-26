#pragma once

#include "Card.h"

class NetworkBackend;

class NetworkCard : public Card
{
public:
  using Card::Card;

  virtual const std::shared_ptr<NetworkBackend> & GetNetworkBackend() const = 0;
};
