#include "settings.h"

#include <QSettings>

namespace
{
    QString getKey(LPCTSTR section, LPCTSTR key)
    {
        const QString qkey = QString::fromUtf8(section) + "/" + QString::fromUtf8(key);
        return qkey;
    }

    QSettings & getOurSettings()
    {
        static QSettings settings;
        return settings;
    }
}

void RegSaveString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPCTSTR buffer)
{
    Q_UNUSED(peruser)
    const QString s = QString::fromUtf8(buffer);
    getOurSettings().setValue(getKey(section, key), QVariant::fromValue(s));
}

void RegSaveValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD value)
{
    Q_UNUSED(peruser)
    getOurSettings().setValue(getKey(section, key), QVariant::fromValue(value));
}

BOOL RegLoadString (LPCTSTR section, LPCTSTR key, BOOL peruser, LPTSTR buffer, DWORD chars)
{
    Q_UNUSED(peruser)

    QSettings & settings = getOurSettings();
    const QString qkey = getKey(section, key);
    if (settings.contains(qkey))
    {
        const QVariant value = settings.value(qkey);
        const std::string s = value.toString().toStdString();

        strncpy(buffer, s.c_str(), chars);
        buffer[chars - 1] = 0;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, DWORD *value)
{
    Q_UNUSED(peruser)

    QSettings & settings = getOurSettings();
    const QString qkey = getKey(section, key);
    if (settings.contains(qkey))
    {
        const QVariant v = settings.value(qkey);
        *value = v.toUInt();
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOL RegLoadValue (LPCTSTR section, LPCTSTR key, BOOL peruser, BOOL *value)
{
    Q_UNUSED(peruser)

    QSettings & settings = getOurSettings();
    const QString qkey = getKey(section, key);
    if (settings.contains(qkey))
    {
        const QVariant v = settings.value(qkey);
        *value = v.toBool();
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}
