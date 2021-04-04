#include "configuration.h"


namespace
{
    QString getKey(const std::string & section, const std::string & key)
    {
        const QString qkey = QString::fromStdString(section) + "/" + QString::fromStdString(key);
        return qkey;
    }
}

std::string Configuration::getString(const std::string & section, const std::string & key) const
{
    const QString qkey = getKey(section, key);
    const QVariant value = mySettings.value(qkey);
    const std::string s = value.toString().toStdString();
    return s;
}

DWORD Configuration::getDWord(const std::string & section, const std::string & key) const
{
    const QString qkey = getKey(section, key);
    const QVariant v = mySettings.value(qkey);
    const uint value = v.toUInt();
    return value;
}

bool Configuration::getBool(const std::string & section, const std::string & key) const
{
    const QString qkey = getKey(section, key);
    const QVariant v = mySettings.value(qkey);
    const bool value = v.toBool();
    return value;
}

void Configuration::putString(const std::string & section, const std::string & key, const std::string & value)
{
    const QString s = QString::fromStdString(value);
    mySettings.setValue(getKey(section, key), QVariant::fromValue(s));
}

void Configuration::putDWord(const std::string & section, const std::string & key, const DWORD value)
{
    mySettings.setValue(getKey(section, key), QVariant::fromValue(value));
}

std::map<std::string, std::map<std::string, std::string>> Configuration::getAllValues() const
{
    throw std::runtime_error("Configuration::getAllValues not implemented.");
}
