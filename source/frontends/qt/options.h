#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "StdAfx.h"
#include "Card.h"
#include "Common.h"

#include <QString>
#include <QColor>

class GlobalOptions
{
public:
    GlobalOptions();  // empty, uninitialised

    static GlobalOptions fromQSettings();

    QString screenshotTemplate;
    QString gamepadName;
    bool gamepadSquaring;

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
    SS_CARDTYPE cardInSlot3;
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
    QString printerFilename;
};

class QtFrame;
void getAppleWinPreferences(PreferenceData & data);
void setAppleWinPreferences(const std::shared_ptr<QtFrame> & frame, const PreferenceData & currentData, const PreferenceData & newData);

#endif // CONFIGURATION_H
