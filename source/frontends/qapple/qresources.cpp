#include "StdAfx.h"

#include <QFile>
#include <QImage>

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

HBITMAP LoadBitmap(HINSTANCE hInstance, const std::string & filename)
{
    QImage * image = nullptr;
    if (!filename.empty())
    {
        const std::string path = ":/resources/" + filename + ".BMP";

        image = new QImage(QString::fromStdString(path));
    }

    if (!image || image->isNull())
    {
        LogFileOutput("LoadBitmap: could not load resource %s\n", filename.c_str());
    }
    return image;
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
    const QImage * image = static_cast<QImage *>(hbit);
    const uchar * bits = image->bits();
    const qsizetype size = image->sizeInBytes();
    const qsizetype requested = cb;

    const qsizetype copied = std::min(requested, size);

    uchar * dest = static_cast<uchar *>(lpvBits);
    for (qsizetype i = 0; i < copied; ++i)
    {
        dest[i] = ~bits[i];
    }
    return copied;
}

BOOL DeleteObject(HGDIOBJ ho)
{
    QImage * image = static_cast<QImage *>(ho);
    delete image;
    return TRUE;
}
