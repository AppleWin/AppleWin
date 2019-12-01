#ifndef VIEWBUFFER_H
#define VIEWBUFFER_H

#include "QHexView/document/buffer/qhexbuffer.h"

class ViewBuffer : public QHexBuffer
{
    Q_OBJECT

public:
    explicit ViewBuffer(QObject *parent = nullptr);

public:
    uchar at(int idx) override;
    void replace(int offset, const QByteArray& data) override;
    void read(char* data, int size) override;
    void read(const QByteArray& ba) override;

public:
    int length() const override;
    void insert(int offset, const QByteArray& data) override;
    void remove(int offset, int length) override;
    QByteArray read(int offset, int length) override;
    void read(QIODevice* iodevice) override;
    void write(QIODevice* iodevice) override;
private:
    QByteArray myData;
};

#endif // VIEWBUFFER_H
