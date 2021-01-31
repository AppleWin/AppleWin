#ifndef REGISTRY_H
#define REGISTRY_H

#include "linux/registry.h"

#include <QSettings>

class Configuration : public Registry
{
public:
    std::string getString(const std::string & section, const std::string & key) const override;
    DWORD getDWord(const std::string & section, const std::string & key) const override;
    bool getBool(const std::string & section, const std::string & key) const override;

    void putString(const std::string & section, const std::string & key, const std::string & value) override;
    void putDWord(const std::string & section, const std::string & key, const DWORD value) override;

private:
    QSettings mySettings;
};

#endif // REGISTRY_H
