#include "audioinfo.h"
#include "qdirectsound.h"
#include "ui_audioinfo.h"

#include "StdAfx.h"
#include "Core.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// no view in Qt5, use a real QString.
typedef QString QLatin1StringView;
#endif

AudioInfo::AudioInfo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AudioInfo)
{
    ui->setupUi(this);
}

AudioInfo::~AudioInfo()
{
    delete ui;
}

void AudioInfo::updateInfo(const qint64 speed, const qint64 target)
{
    ++myCounter;

    if (myCounter % myPeriod != 0)
    {
        return;
    }

    myCounter = 0;
    const std::vector<QDirectSound::SoundInfo> & info = QDirectSound::getAudioInfo();

    QString s("Name   Channels Buffer  Underruns\n");
    for (const auto & i : info)
    {
        if (i.running)
        {
            s += QString("%1   %2   %3   %4\n")
                .arg(QString(i.name.c_str()), -10)
                .arg(i.channels, 2)
                .arg(i.buffer, 4)
                .arg(i.numberOfUnderruns, 8);
        }
    }
    s += QString("\nspeed                = %1\n").arg(speed, 10);
    s +=   QString("target               = %1\n").arg(target, 10);
    s +=   QString("g_nCpuCyclesFeedback =     %1\n").arg(g_nCpuCyclesFeedback, 6);

    ui->info->setPlainText(s);
}
