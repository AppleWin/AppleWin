#pragma once

#include <memory>

class FrameBase;
class Paddle;
class Registry;

void SetFrame(const std::shared_ptr<FrameBase> & frame);

void InitialiseEmulator();
void DestroyEmulator();

// RAII around Frame Registry and Paddle
class Initialisation
{
public:
  Initialisation(
    const std::shared_ptr<FrameBase> & frame,
    const std::shared_ptr<Paddle> & paddle
    );
  ~Initialisation();
};

// RAII around LogInit / LogDone.
class LoggerContext
{
public:
  LoggerContext(const bool log);
  ~LoggerContext();
};

class RegistryContext
{
public:
  RegistryContext(const std::shared_ptr<Registry> & registry);
  ~RegistryContext();
};
