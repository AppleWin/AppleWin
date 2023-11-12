#include "audioinfo.h"
#include "qdirectsound.h"
#include "ui_audioinfo.h"
#include <sstream>

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

void AudioInfo::updateInfo()
{
    ++myCounter;

    if (myCounter % myPeriod != 0)
    {
        return;
    }

    myCounter = 0;
    const std::vector<QDirectSound::SoundInfo> & info = QDirectSound::getAudioInfo();

    QString s("Channels Buffer Underrruns\n");
    for (const auto & i : info)
    {
        if (i.running)
        {
            s += QString("      %1   %2   %3\n")
                .arg(i.channels, 2)
                .arg(i.buffer, 4)
                .arg(i.numberOfUnderruns, 8);
        }
    }

    ui->info->setPlainText(s);
}
