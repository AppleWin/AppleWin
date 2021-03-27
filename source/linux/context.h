#pragma once

#include <memory>

class FrameBase;

void SetFrame(const std::shared_ptr<FrameBase> & frame);

class Initialisation
{
public:
  Initialisation(const std::shared_ptr<FrameBase> & frame);
  ~Initialisation();
};
