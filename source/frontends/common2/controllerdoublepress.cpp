#include "frontends/common2/controllerdoublepress.h"

namespace common2
{

  bool ControllerDoublePress::pressButton()
  {
    const auto secondBtnPress = std::chrono::steady_clock::now();
    const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(secondBtnPress - myFirstBtnPress).count();
    myFirstBtnPress = secondBtnPress;
    return dt <= myThreshold;
  }

}
