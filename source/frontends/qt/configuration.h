#ifndef REGISTRY_H
#define REGISTRY_H

#include "linux/registry.h"

#include <QSettings>

class Configuration : public Registry
{
public:
    virtual std::string getString(const std::string & section, const std::string & key) const;
    virtual DWORD getDWord(const std::string & section, const std::string & key) const;
    virtual bool getBool(const std::string & section, const std::string & key) const;

    virtual void putString(const std::string & section, const std::string & key, const std::string & value);
    virtual void putDWord(const std::string & section, const std::string & key, const DWORD value);

private:
    QSettings mySettings;
};

#endif // REGISTRY_H
