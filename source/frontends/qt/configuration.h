#ifndef REGISTRY_H
#define REGISTRY_H

#include "linux/registryclass.h"

#include <QSettings>

class Configuration : public Registry
{
public:
    std::string getString(const std::string & section, const std::string & key) const override;
    uint32_t getDWord(const std::string & section, const std::string & key) const override;
    bool getBool(const std::string & section, const std::string & key) const override;

    void putString(const std::string & section, const std::string & key, const std::string & value) override;
    void putDWord(const std::string & section, const std::string & key, const uint32_t value) override;

    std::map<std::string, std::map<std::string, std::string>> getAllValues() const override;

private:
    QSettings mySettings;

    QVariant getVariant(const std::string & section, const std::string & key) const;
};

#endif // REGISTRY_H
