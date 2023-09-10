#include "frontends/common2/controllerquit.h"

namespace common2
{

  bool ControllerQuit::pressButton()
  {
    const auto secondBtnPress = std::chrono::steady_clock::now();
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(secondBtnPress - myFirstBtnPress).count();
    myFirstBtnPress = secondBtnPress;
    return dt <= myThreshold;
  }

}
