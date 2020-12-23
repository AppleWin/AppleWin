#include "viewbuffer.h"

ViewBuffer::ViewBuffer(QObject *parent) : QHexBuffer(parent)
{

}

uchar ViewBuffer::at(int idx)
{
    return static_cast<uchar>(myData.at(idx));
}

void ViewBuffer::replace(int offset, const QByteArray& data)
{
    Q_UNUSED(offset)
    Q_UNUSED(data)
}

void ViewBuffer::read(char* data, int size)
{
    myData.setRawData(data, static_cast<uint>(size));
}

void ViewBuffer::read(const QByteArray& ba)
{
    Q_UNUSED(ba)
}

int ViewBuffer::length() const
{
    return myData.length();
}

void ViewBuffer::insert(int offset, const QByteArray& data)
{
    Q_UNUSED(offset)
    Q_UNUSED(data)
}

void ViewBuffer::remove(int offset, int length)
{
    Q_UNUSED(offset)
    Q_UNUSED(length)
}

QByteArray ViewBuffer::read(int offset, int length)
{
    return myData.mid(offset, length);
}

void ViewBuffer::read(QIODevice* iodevice)
{
    Q_UNUSED(iodevice)
}

void ViewBuffer::write(QIODevice* iodevice)
{
    Q_UNUSED(iodevice)
}
