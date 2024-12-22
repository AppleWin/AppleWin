#include "configuration.h"


namespace
{
    QString getKey(const std::string & section, const std::string & key)
    {
        const QString qkey = QString::fromStdString(section) + "/" + QString::fromStdString(key);
        return qkey;
    }
}

QVariant Configuration::getVariant(const std::string & section, const std::string & key) const
{
    const QString qkey = getKey(section, key);
    const QVariant value = mySettings.value(qkey);
    if (value.isNull())
    {
        throw std::runtime_error("Missing " + qkey.toStdString());
    }
    return value;
}

std::string Configuration::getString(const std::string & section, const std::string & key) const
{
    const QVariant value = getVariant(section, key);
    const std::string res = value.toString().toStdString();
    return res;
}

uint32_t Configuration::getDWord(const std::string & section, const std::string & key) const
{
    const QVariant value = getVariant(section, key);
    const uint res = value.toUInt();
    return res;
}

bool Configuration::getBool(const std::string & section, const std::string & key) const
{
    const QVariant value = getVariant(section, key);
    const bool res = value.toBool();
    return res;
}

void Configuration::putString(const std::string & section, const std::string & key, const std::string & value)
{
    const QString s = QString::fromStdString(value);
    mySettings.setValue(getKey(section, key), QVariant::fromValue(s));
}

void Configuration::putDWord(const std::string & section, const std::string & key, const uint32_t value)
{
    mySettings.setValue(getKey(section, key), QVariant::fromValue(value));
}

std::map<std::string, std::map<std::string, std::string>> Configuration::getAllValues() const
{
    throw std::runtime_error("Configuration::getAllValues not implemented.");
}
