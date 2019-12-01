#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "preferences.h"

class GlobalOptions
{
public:
    GlobalOptions();  // empty, uninitialised

    static GlobalOptions fromQSettings();

    QString screenshotTemplate;
    QString gamepadName;

    int slot0Card;
    int ramWorksMemorySize;
    int msGap;
    int msFullSpeed;

    int audioLatency;
    int silenceDelay;
    int volume;

    void getData(Preferences::Data & data) const;
    void setData(const Preferences::Data & data);
};

void getAppleWinPreferences(Preferences::Data & data);
void setAppleWinPreferences(const Preferences::Data & currentData, const Preferences::Data & newData);

#endif // CONFIGURATION_H
