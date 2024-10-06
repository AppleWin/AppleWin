#ifndef VIEWBUFFER_H
#define VIEWBUFFER_H

#include <QHexView/qhexview.h>

class ViewBuffer : public QHexBuffer
{
    Q_OBJECT

public:
    explicit ViewBuffer(QObject *parent = nullptr);

public:
    uchar at(qint64 idx) override;
    void replace(qint64 offset, const QByteArray& data) override;
    void read(char* data, int size) override;
    void read(const QByteArray& ba) override;

public:
    qint64 length() const override;
    void insert(qint64 offset, const QByteArray& data) override;
    void remove(qint64 offset, int length) override;
    QByteArray read(qint64 offset, int length) override;
    bool read(QIODevice* iodevice) override;
    void write(QIODevice* iodevice) override;

    qint64 indexOf(const QByteArray& ba, qint64 from) override;
    qint64 lastIndexOf(const QByteArray& ba, qint64 from) override;
private:
    QByteArray myData;
};

#endif // VIEWBUFFER_H
