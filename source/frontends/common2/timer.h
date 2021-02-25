#include <chrono>
#include <iosfwd>

namespace common2
{

  class Timer
  {
  public:
    Timer();
    void tic();
    void toc();

    double getTimeInSeconds() const;

    friend std::ostream& operator<<(std::ostream& os, const Timer & timer);

  private:

    std::chrono::time_point<std::chrono::steady_clock> myT0;

    double mySum;
    double mySum2;
    int myN;
  };

  std::ostream& operator<<(std::ostream& os, const Timer & timer);

}
