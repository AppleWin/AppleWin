#include "qdirectsound.h"

#include "loggingcategory.h"
#include "qdirectsound.h"
#include "windows.h"
#include "linux/linuxinterface.h"
#include <unordered_map>
#include <memory>

#include <QAudioOutput>

namespace
{

    class DirectSoundGenerator : public IDirectSoundBuffer
    {
    public:
        DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc);
        virtual ~DirectSoundGenerator() override;

        void writeAudio();
        void setOptions(const qint32 initialSilence);

        virtual HRESULT Release() override;
        virtual HRESULT Stop() override;
        virtual HRESULT Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags ) override;
        virtual HRESULT SetVolume( LONG lVolume );

    private:
        typedef short int audio_t;

        std::shared_ptr<QAudioOutput> myAudioOutput;
        QAudioFormat myAudioFormat;
        QIODevice * myDevice;

        // options
        qint32 myInitialSilence;

        void writeEnoughSilence(const qint64 ms);
    };


    std::unordered_map<IDirectSoundBuffer *, std::shared_ptr<DirectSoundGenerator> > activeSoundGenerators;

    DirectSoundGenerator::DirectSoundGenerator(LPCDSBUFFERDESC lpcDSBufferDesc) : IDirectSoundBuffer(lpcDSBufferDesc)
    {
        myInitialSilence = 200;

        // only initialise here to skip all the buffers which are not in DSBSTATUS_PLAYING mode
        QAudioFormat audioFormat;
        audioFormat.setSampleRate(mySampleRate);
        audioFormat.setChannelCount(myChannels);
        audioFormat.setSampleSize(myBitsPerSample);
        audioFormat.setCodec(QString::fromUtf8("audio/pcm"));
        audioFormat.setByteOrder(QAudioFormat::LittleEndian);
        audioFormat.setSampleType(QAudioFormat::SignedInt);

        myAudioOutput = std::make_shared<QAudioOutput>(audioFormat);
        myAudioFormat = myAudioOutput->format();
    }

    DirectSoundGenerator::~DirectSoundGenerator()
    {
        myAudioOutput->stop();
        myDevice = nullptr;
    }

    HRESULT DirectSoundGenerator::Release()
    {
        activeSoundGenerators.erase(this);
        return IUnknown::Release();
    }

    void DirectSoundGenerator::setOptions(const qint32 initialSilence)
    {
        myInitialSilence = std::max(0, initialSilence);
    }

    HRESULT DirectSoundGenerator::SetVolume( LONG lVolume )
    {
        const HRESULT res = IDirectSoundBuffer::SetVolume(lVolume);
        const qreal logVolume = GetLogarithmicVolume();
        const qreal linVolume = QAudio::convertVolume(logVolume, QAudio::LogarithmicVolumeScale, QAudio::LinearVolumeScale);
        myAudioOutput->setVolume(linVolume);
        return res;
    }

    HRESULT DirectSoundGenerator::Stop()
    {
        const HRESULT res = IDirectSoundBuffer::Stop();
        myAudioOutput->stop();
        myDevice = nullptr;

        return res;
    }

    HRESULT DirectSoundGenerator::Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags )
    {
        const HRESULT res = IDirectSoundBuffer::Play(dwReserved1, dwReserved2, dwFlags);
        myDevice = myAudioOutput->start();

        const int bytesSize = myAudioOutput->bufferSize();
        const qint32 frameSize = myAudioFormat.framesForBytes(bytesSize);

        const qint32 framePeriod = myAudioFormat.framesForBytes(myAudioOutput->periodSize());
        qDebug(appleAudio) << "AudioOutput: size =" << frameSize << "f, period =" << framePeriod << "f";

        writeEnoughSilence(myInitialSilence); // ms
        return res;
    }

    void DirectSoundGenerator::writeEnoughSilence(const qint64 ms)
    {
        // write a few ms of silence
        const qint32 framesSilence = myAudioFormat.framesForDuration(ms * 1000);  // target frames to write

        const qint32 bytesFree = myAudioOutput->bytesFree();
        const qint32 framesFree = myAudioFormat.framesForBytes(bytesFree);  // number of frames avilable to write
        const qint32 framesToWrite = std::min(framesFree, framesSilence);
        const qint64 bytesToWrite = myAudioFormat.bytesForFrames(framesToWrite);

        std::vector<char> silence(bytesToWrite);
        myDevice->write(silence.data(), silence.size());

        const qint64 duration = myAudioFormat.durationForFrames(framesToWrite);
        qDebug(appleAudio) << "Written some silence: frames =" << framesToWrite << ", duration =" << duration / 1000 << "ms";
    }

    void DirectSoundGenerator::writeAudio()
    {
        if (myDevice)
        {
            // we write all we have available (up to the free bytes)
            const DWORD bytesFree = myAudioOutput->bytesFree();

            LPVOID lpvAudioPtr1, lpvAudioPtr2;
            DWORD dwAudioBytes1, dwAudioBytes2;
            // this function reads as much as possible up to bytesFree
            Read(bytesFree, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

            qint64 bytesWritten = 0;
            qint64 bytesToWrite = 0;
            if (lpvAudioPtr1)
            {
                bytesWritten += myDevice->write((char *)lpvAudioPtr1, dwAudioBytes1);
                bytesToWrite += dwAudioBytes1;
            }
            if (lpvAudioPtr2)
            {
                bytesWritten += myDevice->write((char *)lpvAudioPtr2, dwAudioBytes2);
                bytesToWrite += dwAudioBytes2;
            }

            if (bytesToWrite != bytesWritten)
            {
                qDebug(appleAudio) << "Mismatch:" << bytesToWrite << "!=" << bytesWritten;
            }
        }
    }
}

IDirectSoundBuffer * iCreateDirectSoundBuffer(LPCDSBUFFERDESC lpcDSBufferDesc)
{
    try
    {
        std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(lpcDSBufferDesc);
        DirectSoundGenerator * ptr = generator.get();
        activeSoundGenerators[ptr] = generator;
        return ptr;
    }
    catch (const std::exception & e)
    {
        qDebug(appleAudio) << "IDirectSoundBuffer: " << e.what();
        return nullptr;
    }
}

namespace QDirectSound
{

    void writeAudio()
    {
        for (const auto & it : activeSoundGenerators)
        {
            const auto generator = it.second;
            generator->writeAudio();
        }
    }

    void setOptions(const qint32 initialSilence)
    {
        for (const auto & it : activeSoundGenerators)
        {
            const auto generator = it.second;
            generator->setOptions(initialSilence);
        }
    }

}
