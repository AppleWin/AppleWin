#include "linux/context.h"

namespace common2
{
  struct EmulatorOptions;

  class CommonInitialisation : public Initialisation
  {
  public:
    CommonInitialisation(
      const std::shared_ptr<LinuxFrame> & frame,
      const std::shared_ptr<Paddle> & paddle,
      const EmulatorOptions & options
      );
    ~CommonInitialisation();
  };
}
