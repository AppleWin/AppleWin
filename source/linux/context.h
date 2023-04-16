#pragma once

#include <memory>

class FrameBase;
class LinuxFrame;
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
    const std::shared_ptr<LinuxFrame> & frame,
    const std::shared_ptr<Paddle> & paddle
    );
  ~Initialisation();
protected:
  const std::shared_ptr<LinuxFrame> myFrame;
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
