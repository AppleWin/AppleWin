#pragma once

#include <memory>

class FrameBase;

void SetFrame(const std::shared_ptr<FrameBase> & frame);

// RAII around Frame Registry and Paddle
class Initialisation
{
public:
  Initialisation(const std::shared_ptr<FrameBase> & frame);
  ~Initialisation();
};

// RAII around LogInit / LogDone.
class Logger
{
public:
  Logger(const bool log);
  ~Logger();
};
