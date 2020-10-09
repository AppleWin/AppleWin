#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "StdAfx.h"
#include "Card.h"
#include "Common.h"
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

    void setData(const GlobalOptions & data);
};

struct PreferenceData
{
    GlobalOptions options;

    eApple2Type apple2Type;
    SS_CARDTYPE cardInSlot4;
    SS_CARDTYPE cardInSlot5;
    bool hdInSlot7;
    bool hz50;

    bool enhancedSpeed;

    int videoType;
    bool scanLines;
    bool verticalBlend;
    int speakerVolume;
    int mockingboardVolume;

    std::vector<QString> disks;
    std::vector<QString> hds;

    QColor monochromeColor;

    QString saveState;
};

void getAppleWinPreferences(PreferenceData & data);
void setAppleWinPreferences(const PreferenceData & currentData, const PreferenceData & newData);

#endif // CONFIGURATION_H
