#include "StdAfx.h"

#include <QFile>
#include <QImage>

#include "Log.h"

HRSRC FindResource(void *, const char * filename, const char *)
{
    HRSRC result;

    if (filename)
    {
        const std::string path = std::string(":/resources/") + filename;

        QFile res(QString::fromStdString(path));
        if (res.exists() && res.open(QIODevice::ReadOnly))
        {
            QByteArray data = res.readAll();
            result.data.assign(data.begin(), data.end());
        }
    }

    if (result.data.empty())
    {
          LogFileOutput("FindResource: could not load resource %s\n", filename);
    }

    return result;
}

struct CBITMAP : public CHANDLE
{
    QImage image;
};

HBITMAP LoadBitmap(HINSTANCE hInstance, const char * filename)
{
    Q_UNUSED(hInstance)

    QImage image;
    if (filename)
    {
        const std::string path = std::string(":/resources/") + filename + ".BMP";
        image = QImage(QString::fromStdString(path));
    }

    if (image.isNull())
    {
        LogFileOutput("LoadBitmap: could not load resource %s\n", filename);
        return nullptr;
    }
    else
    {
        CBITMAP * bitmap = new CBITMAP;
        bitmap->image = image;
        return bitmap;
    }
}

LONG GetBitmapBits(HBITMAP hbit, LONG cb, LPVOID lpvBits)
{
    const CBITMAP & bitmap = dynamic_cast<CBITMAP&>(*hbit);
    const uchar * bits = bitmap.image.bits();
    const qsizetype size = bitmap.image.sizeInBytes();
    const qsizetype requested = cb;

    const qsizetype copied = std::min(requested, size);

    uchar * dest = static_cast<uchar *>(lpvBits);
    for (qsizetype i = 0; i < copied; ++i)
    {
        dest[i] = ~bits[i];
    }
    return copied;
}
