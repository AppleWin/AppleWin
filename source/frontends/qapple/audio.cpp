#include "StdAfx.h"

#include "audio.h"

#include "Common.h"
#include "CPU.h"
#include "Applewin.h"
#include "Memory.h"
#include "loggingcategory.h"

#include <QDebug>

// Speaker

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG uExecutedCycles)
{
    CpuCalcCycles(uExecutedCycles);

    Q_UNUSED(pc)
    Q_UNUSED(addr)
    Q_UNUSED(bWrite)
    Q_UNUSED(d)

    AudioGenerator::instance().toggle();

    return MemReadFloatingBus(uExecutedCycles);
}

AudioGenerator & AudioGenerator::instance()
{
    static std::shared_ptr<AudioGenerator> audioGenerator(new AudioGenerator());
    return *audioGenerator;
}

AudioGenerator::AudioGenerator()
{
    myDevice = nullptr;
    myInitialSilence = 200;
    mySilenceDelay = 10000;
    myPhysicalReference = 0x0fff;

    QAudioFormat audioFormat;
    audioFormat.setSampleRate(44100);
    audioFormat.setChannelCount(1);
    audioFormat.setSampleSize(sizeof(audio_t) * 8);
    audioFormat.setCodec(QString::fromUtf8("audio/pcm"));
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setSampleType(QAudioFormat::SignedInt);

    myAudioOutput.reset(new QAudioOutput(audioFormat));
    myAudioFormat = myAudioOutput->format();
}

QAudioOutput * AudioGenerator::getAudioOutput()
{
    return myAudioOutput.get();
}

void AudioGenerator::getOptions(qint32 & initialSilence, qint32 & silenceDelay, qint32 & physical) const
{
    initialSilence = myInitialSilence;
    silenceDelay = mySilenceDelay;
    physical = myPhysicalReference;
}

void AudioGenerator::setOptions(const qint32 initialSilence, const qint32 silenceDelay, const qint32 physical)
{
    myInitialSilence = std::max(0, initialSilence);
    mySilenceDelay = std::max(0, silenceDelay);
    myPhysicalReference = std::max(0, physical);
}

void AudioGenerator::stateChanged(QAudio::State state)
{
    qDebug(appleAudio) << "Changed state: state =" << state << ", error =" << myAudioOutput->error() << ", free =" << myAudioOutput->bytesFree() << "bytes";
}

qint64 AudioGenerator::toFrameTime(qint64 cpuCycples)
{
    const double CLKS_PER_SEC = g_fCurrentCLK6502;
    qint64 timeInFrames = (cpuCycples - myStartCPUCycles) * myAudioFormat.sampleRate() / CLKS_PER_SEC;
    return timeInFrames;
}

void AudioGenerator::toggle()
{
    const qint64 timeInFrames = toFrameTime(g_nCumulativeCycles);
    myTicks.push(timeInFrames);
}

bool AudioGenerator::isRunning()
{
    QAudio::State state = myAudioOutput->state();
    QAudio::Error error = myAudioOutput->error();
    if (state == QAudio::ActiveState)
    {
        return true;
    }
    if (state == QAudio::IdleState && error == QAudio::NoError)
    {
        return true;
    }
    return false;
}

void AudioGenerator::start()
{
    if (isRunning())
    {
        return;
    }

    qDebug(appleAudio) << "Restarting the AudioGenerator";

    // restart as we are either starting or recovering from underrun
    myDevice = myAudioOutput->start();

    const int bytesSize = myAudioOutput->bufferSize();
    const qint32 frameSize = myAudioFormat.framesForBytes(bytesSize);

    myBuffer.resize(frameSize);
    myStartCPUCycles = g_nCumulativeCycles;
    myPreviousFrameTime = 0;

    const qint32 framePeriod = myAudioFormat.framesForBytes(myAudioOutput->periodSize());
    qDebug(appleAudio) << "AudioOutput: size =" << frameSize << "f, period =" << framePeriod << "f";

    mySilence = 0;
    myMaximum = 0;
    myValue = 0;
    myPhysical = myPhysicalReference;
    myTicks = std::queue<qint64>();

    writeEnoughSilence(myInitialSilence); // ms
}

void AudioGenerator::writeEnoughSilence(const qint64 ms)
{
    // write a few ms of silence
    const qint32 framesSilence = myAudioFormat.framesForDuration(ms * 1000);  // target frames to write

    const qint32 bytesFree = myAudioOutput->bytesFree();
    const qint32 framesFree = myAudioFormat.framesForBytes(bytesFree);  // number of frames avilable to write

    const qint32 framesToWrite = std::min(framesFree, framesSilence);

    generateSilence(myBuffer.data(), myBuffer.data() + framesToWrite);

    const qint64 bytesToWrite = myAudioFormat.bytesForFrames(framesToWrite);
    const char * data = reinterpret_cast<char *>(myBuffer.data());
    myDevice->write(data, bytesToWrite);

    const qint64 duration = myAudioFormat.durationForFrames(framesToWrite);
    qDebug(appleAudio) << "Written some silence: frames =" << framesToWrite << ", duration =" << duration / 1000 << "ms";
}

void AudioGenerator::stop()
{
    if (!isRunning())
    {
        return;
    }

    const qint32 bytesFree = myAudioOutput->bytesFree();
    const qint32 framesFree = myAudioFormat.framesForBytes(bytesFree);

    // fill with zeros and stop
    generateSilence(myBuffer.data(), myBuffer.data() + framesFree);

    const qint32 bytesToWrite = myAudioFormat.bytesForFrames(framesFree);
    const char * data = reinterpret_cast<char *>(myBuffer.data());
    myDevice->write(data, bytesToWrite);

    const qint64 duration = myAudioFormat.durationForFrames(framesFree);
    qDebug(appleAudio) << "Stopping with silence: frames =" << framesFree << ", duration =" << duration / 1000 << "ms";
    myAudioOutput->stop();
}

void AudioGenerator::writeAudio()
{
    if (!isRunning())
    {
        return;
    }

    // we write all we have available (up to the free bytes)

    const qint64 currentFrameTime = toFrameTime(g_nCumulativeCycles);
    const qint64 newFramesAvailable = currentFrameTime - myPreviousFrameTime;

    const qint32 bytesFree = myAudioOutput->bytesFree();
    const qint64 framesFree = myAudioFormat.framesForBytes(bytesFree);

    const qint64 framesToWrite = std::min(framesFree, newFramesAvailable);

    generateSamples(myBuffer.data(), framesToWrite);

    const qint32 bytesToWrite = myAudioFormat.bytesForFrames(framesToWrite);
    const char * data = reinterpret_cast<char *>(myBuffer.data());
    const qint64 bytesWritten = myDevice->write(data, bytesToWrite);
    if (bytesToWrite != bytesWritten)
    {
        qDebug(appleAudio) << "Mismatch:" << bytesToWrite << "!=" << bytesWritten;
    }
    const qint64 framesWritten = framesToWrite;
    myPreviousFrameTime += framesWritten;

    const qint32 bytesFreeNow = myAudioOutput->bytesFree();
    if (bytesFreeNow > myMaximum)
    {
        // if this number is too big, it probably means we aren't providing enough data
        const qint32 bytesSize = myAudioOutput->bufferSize();
        myMaximum = bytesFreeNow;
        qDebug(appleAudio) << "Running maximum free bytes:" << myMaximum << "/" << bytesSize;
    }
}

void AudioGenerator::generateSilence(audio_t * begin, audio_t * end)
{
    if (myValue != 0)
    {
        const audio_t delta = myPhysical > 0 ? -1 : +1;
        for (audio_t * ptr = begin; ptr != end; ++ptr)
        {
            ++mySilence;
            if (myValue != 0 && mySilence > mySilenceDelay)
            {
                myValue += delta;
            }
            *ptr = myValue;
        }
    }
    else
    {
        // no need to update mySilence as myValue is already 0
        std::fill(begin, end, myValue);
    }
}

void AudioGenerator::generateSamples(audio_t *data, qint64 framesToWrite)
{
    qint64 start = myPreviousFrameTime;
    qint64 end = myPreviousFrameTime + framesToWrite;

    audio_t * head = data;
    while (!myTicks.empty() && (myTicks.front() < end))
    {
        qint64 next = myTicks.front() - start;
        audio_t * last = data + next;
        std::fill(head, last, myValue);
        head = last;
        myPhysical = -myPhysical;
        myValue = myPhysical;
        mySilence = 0;
        myTicks.pop();
    }

    generateSilence(head, data + framesToWrite);
}
