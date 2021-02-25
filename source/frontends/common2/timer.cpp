#include "frontends/common2/timer.h"

#include <ostream>
#include <cmath>
#include <iomanip>

namespace common2
{

  Timer::Timer()
    : mySum(0)
    , mySum2(0)
    , myN(0)
  {
    tic();
  }

  void Timer::tic()
  {
    myT0 = std::chrono::steady_clock::now();
  }

  void Timer::toc()
  {
    const auto now = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - myT0).count();
    const double s = micros * 0.000001;
    mySum += s;
    mySum2 += s * s;
    ++myN;
    myT0 = now;
  }

  double Timer::getTimeInSeconds() const
  {
    return mySum;
  }

  std::ostream& operator<<(std::ostream& os, const Timer & timer)
  {
    const int width = 10;
    const double m1 = timer.mySum / timer.myN;
    const double m2 = timer.mySum2 / timer.myN;
    const double std = std::sqrt(std::max(0.0, m2 - m1 * m1));
    const double scale = 1000;
    os << "total = " << std::setw(width) << timer.mySum * scale << " ms";
    os << ", mean = " << std::setw(width) << m1 * scale << " ms";
    os << ", std = " << std::setw(width) << std * scale << " ms";
    os << ", n = " << std::setw(6) << timer.myN;
    return os;
  }

}
