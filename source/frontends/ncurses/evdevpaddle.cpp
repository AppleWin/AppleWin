#include "StdAfx.h"
#include "frontends/ncurses/evdevpaddle.h"

#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "applen_config.h"

#ifdef LIBEVDEV_FOUND
#include <libevdev/libevdev.h>
#endif

#include "Log.h"

namespace na2
{

    EvDevPaddle::EvDevPaddle(const std::string &device)
        : myFD(0)
        , myButtonCodes(2)
        , myAxisCodes(2)
        , myAxisMins(2)
        , myAxisMaxs(2)
    {
#ifdef LIBEVDEV_FOUND
        myFD = open(device.c_str(), O_RDONLY | O_NONBLOCK);
        if (myFD > 0)
        {
            libevdev *dev;
            int rc = libevdev_new_from_fd(myFD, &dev);
            if (rc < 0)
            {
                LogFileOutput("Input: failed to init libevdev (%s): %s\n", strerror(-rc), device.c_str());
            }
            else
            {
                myDev.reset(dev, libevdev_free);

                myName = libevdev_get_name(dev);

                myButtonCodes[0] = BTN_SOUTH;
                myButtonCodes[1] = BTN_EAST;
                myAxisCodes[0] = ABS_X;
                myAxisCodes[1] = ABS_Y;

                for (size_t i = 0; i < myAxisCodes.size(); ++i)
                {
                    myAxisMins[i] = libevdev_get_abs_minimum(dev, myAxisCodes[i]);
                    myAxisMaxs[i] = libevdev_get_abs_maximum(dev, myAxisCodes[i]);
                }
            }
        }
        else
        {
            LogFileOutput("Input: failed to open device (%s): %s\n", strerror(errno), device.c_str());
        }
#endif
    }

    EvDevPaddle::~EvDevPaddle()
    {
        if (myFD > 0)
        {
            close(myFD);
        }
    }

    int EvDevPaddle::poll()
    {
        int counter = 0;
        if (!myDev)
        {
            return counter;
        }

#ifdef LIBEVDEV_FOUND
        input_event ev;
        int rc = LIBEVDEV_READ_STATUS_SUCCESS;
        do
        {
            if (rc == LIBEVDEV_READ_STATUS_SYNC)
                rc = libevdev_next_event(myDev.get(), LIBEVDEV_READ_FLAG_SYNC, &ev);
            else
                rc = libevdev_next_event(myDev.get(), LIBEVDEV_READ_FLAG_NORMAL, &ev);
            ++counter;
        } while (rc >= 0);
#endif

        return counter;
    }

    const std::string &EvDevPaddle::getName() const
    {
        return myName;
    }

    bool EvDevPaddle::getButton(int i) const
    {
        int value = 0;
        if (myDev)
        {
#ifdef LIBEVDEV_FOUND
            int rc = libevdev_fetch_event_value(myDev.get(), EV_KEY, myButtonCodes[i], &value);
#endif
        }
        return value != 0;
    }

    double EvDevPaddle::getAxis(int i) const
    {
#ifdef LIBEVDEV_FOUND
        if (myDev)
        {
            int value = 0;
            int rc = libevdev_fetch_event_value(myDev.get(), EV_ABS, myAxisCodes[i], &value);
            const double axis = 2.0 * (value - myAxisMins[i]) / (myAxisMaxs[i] - myAxisMins[i]) - 1.0;
            return axis;
        }
        else
#endif
        {
            return 0;
        }
    }

} // namespace na2
