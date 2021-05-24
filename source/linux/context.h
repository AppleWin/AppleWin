#pragma once

#include <memory>

class FrameBase;
class Paddle;
class Registry;

void SetFrame(const std::shared_ptr<FrameBase> & frame);

// RAII around Frame Registry and Paddle
class Initialisation
{
public:
  Initialisation(
    const std::shared_ptr<Registry> & registry,
    const std::shared_ptr<FrameBase> & frame,
    const std::shared_ptr<Paddle> & paddle
    );
  ~Initialisation();
};

// RAII around LogInit / LogDone.
class Logger
{
public:
  Logger(const bool log);
  ~Logger();
};
