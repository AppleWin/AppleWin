#ifndef AUDIO_H
#define AUDIO_H

#include <QIODevice>
#include <QAudioFormat>
#include <QAudioOutput>
#include <queue>
#include <memory>
#include <vector>

class AudioGenerator {
public:

    static AudioGenerator & instance();

    QAudioOutput * getAudioOutput();

    void toggle();
    void start();
    void stop();
    void writeAudio();
    void stateChanged(QAudio::State state);

private:
    typedef short int audio_t;

    std::queue<qint64> myTicks;

    std::shared_ptr<QAudioOutput> myAudioOutput;
    QAudioFormat myAudioFormat;
    QIODevice * myDevice;
    std::vector<audio_t> myBuffer;

    qint64 myStartCPUCycles;
    qint64 myPreviousFrameTime;
    qint32 myMaximum;

    qint32 mySilence;
    audio_t myPhysical;
    audio_t myValue;

    AudioGenerator();

    qint64 toFrameTime(qint64 cpuCycples);
    void generateSamples(audio_t *data, qint64 framesToWrite);
    void writeEnoughSilence(const qint64 ms);
    void generateSilence(audio_t * begin, audio_t * end);
    bool isRunning();
};

#endif // AUDIO_H
