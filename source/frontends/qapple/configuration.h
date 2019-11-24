#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <memory>

#include "preferences.h"

class QGamepad;

QString getScreenshotTemplate();
int getSlot0Card();
int getRamWorksMemorySize();

Preferences::Data getCurrentPreferenceData(const std::shared_ptr<QGamepad> & gamepad);
void setNewPreferenceData(const Preferences::Data & currentData, const Preferences::Data & newData,
                          std::shared_ptr<QGamepad> & gamepad);

#endif // CONFIGURATION_H
