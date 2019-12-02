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

    void setData(const GlobalOptions & data);
};

struct PreferenceData
{
    GlobalOptions options;

    int apple2Type;
    bool mouseInSlot4;
    bool cpmInSlot5;
    bool hdInSlot7;
    bool hz50;

    bool enhancedSpeed;

    int videoType;
    bool scanLines;
    bool verticalBlend;

    std::vector<QString> disks;
    std::vector<QString> hds;

    QString saveState;
};

void getAppleWinPreferences(PreferenceData & data);
void setAppleWinPreferences(const PreferenceData & currentData, const PreferenceData & newData);

#endif // CONFIGURATION_H
