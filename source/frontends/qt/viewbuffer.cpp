#include "viewbuffer.h"

ViewBuffer::ViewBuffer(QObject *parent)
    : QHexBuffer(parent)
{
}

uchar ViewBuffer::at(qint64 idx)
{
    return static_cast<uchar>(myData.at(idx));
}

void ViewBuffer::replace(qint64 offset, const QByteArray &data)
{
    Q_UNUSED(offset)
    Q_UNUSED(data)
}

void ViewBuffer::read(char *data, int size)
{
    myData.setRawData(data, static_cast<uint>(size));
}

void ViewBuffer::read(const QByteArray &ba)
{
    Q_UNUSED(ba)
}

qint64 ViewBuffer::length() const
{
    return myData.length();
}

void ViewBuffer::insert(qint64 offset, const QByteArray &data)
{
    Q_UNUSED(offset)
    Q_UNUSED(data)
}

void ViewBuffer::remove(qint64 offset, int length)
{
    Q_UNUSED(offset)
    Q_UNUSED(length)
}

QByteArray ViewBuffer::read(qint64 offset, int length)
{
    return myData.mid(offset, length);
}

bool ViewBuffer::read(QIODevice *iodevice)
{
    Q_UNUSED(iodevice)
    return false;
}

void ViewBuffer::write(QIODevice *iodevice)
{
    Q_UNUSED(iodevice)
}

qint64 ViewBuffer::indexOf(const QByteArray &ba, qint64 from)
{
    return myData.indexOf(ba, static_cast<int>(from));
}

qint64 ViewBuffer::lastIndexOf(const QByteArray &ba, qint64 from)
{
    return myData.lastIndexOf(ba, static_cast<int>(from));
}
