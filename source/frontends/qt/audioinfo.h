#ifndef AUDIOINFO_H
#define AUDIOINFO_H

#include <QWidget>

namespace Ui
{
    class AudioInfo;
}

class AudioInfo : public QWidget
{
    Q_OBJECT

public:
    explicit AudioInfo(QWidget *parent = nullptr);
    ~AudioInfo();

public slots:
    void updateInfo(const qint64 speed, const qint64 target);

private:
    Ui::AudioInfo *ui;

    const int myPeriod = 4;
    int myCounter = 0;
};

#endif // AUDIOINFO_H
