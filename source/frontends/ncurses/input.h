#include <string>
#include <memory>
#include <vector>

struct libevdev;
struct input_event;

class Input
{
public:
  Input(const std::string & device);
  ~Input();

  int poll();

  bool getButton(int i) const;
  int getAxis(int i) const;

private:
  int myFD;
  std::shared_ptr<libevdev> myDev;

  void process(const input_event & ev);

  std::vector<unsigned int> myButtonCodes;
  std::vector<unsigned int> myAxisCodes;
  std::vector<int> myAxisMins;
  std::vector<int> myAxisMaxs;
};
