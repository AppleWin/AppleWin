#include "StdAfx.h"

#include <QFile>

#include "Log.h"

HRSRC FindResource(void *, const std::string & filename, const char *)
{
    HRSRC result;

    if (!filename.empty())
    {
        const std::string path = ":/resources/" + filename;

        QFile res(QString::fromStdString(path));
        if (res.exists() && res.open(QIODevice::ReadOnly))
        {
            QByteArray data = res.readAll();
            result.data.assign(data.begin(), data.end());
        }
    }

    if (result.data.empty())
    {
          LogFileOutput("FindResource: could not load resource %s\n", filename.c_str());
    }

    return result;
}
