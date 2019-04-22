#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <memory>

#include "preferences.h"

class QGamepad;

QString getScreenshotTemplate();
int getSlot0Card();
int getRamWorksMemorySize();

Preferences::Data getCurrentOptions(const std::shared_ptr<QGamepad> & gamepad);
void setNewOptions(const Preferences::Data & currentOptions, const Preferences::Data & newOptions,
                   std::shared_ptr<QGamepad> & gamepad);

#endif // CONFIGURATION_H
