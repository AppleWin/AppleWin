#include "StdAfx.h"
#include "frontends/libretro/rdirectsound.h"
#include "frontends/libretro/environment.h"

#include "linux/linuxsoundbuffer.h"

#include <unordered_set>
#include <cmath>

namespace
{

    class DirectSoundGenerator : public LinuxSoundBuffer
    {
    public:
        DirectSoundGenerator(DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName);
        virtual ~DirectSoundGenerator() override;

        void writeAudio(const size_t fps, const bool write);

        bool isRunning();
        bool isSameFormat(const size_t sampleRate, const size_t channels) const;
        void advanceOneFrame(const size_t fps);

    private:
        std::vector<int16_t> myMixerBuffer;

        void mixBuffer(const void *ptr, const size_t size);
    };

    std::unordered_set<DirectSoundGenerator *> activeSoundGenerators;

    DirectSoundGenerator::DirectSoundGenerator(
        DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName)
        : LinuxSoundBuffer(dwBufferSize, nSampleRate, nChannels, pszVoiceName)
    {
    }

    DirectSoundGenerator::~DirectSoundGenerator()
    {
        activeSoundGenerators.erase(this);
    }

    bool DirectSoundGenerator::isRunning()
    {
        DWORD dwStatus;
        GetStatus(&dwStatus);
        if (dwStatus & DSBSTATUS_PLAYING)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    bool DirectSoundGenerator::isSameFormat(const size_t sampleRate, const size_t channels) const
    {
        return (mySampleRate == sampleRate) && (myChannels == channels);
    }

    void DirectSoundGenerator::advanceOneFrame(const size_t fps)
    {
        const size_t bytesPerFrame = myChannels * sizeof(int16_t);
        const size_t bytesToRead = mySampleRate * bytesPerFrame / fps;

        // it does not matter if we read broken frames, this generator will never play sound

        LPVOID lpvAudioPtr1, lpvAudioPtr2;
        DWORD dwAudioBytes1, dwAudioBytes2;
        Read(bytesToRead, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);
    }

    void mixBuffer(LinuxSoundBuffer *generator, LPVOID lpvAudioPtr, DWORD dwAudioBytes, int16_t *ptr)
    {
        // mix the buffer
        const int16_t *data = static_cast<const int16_t *>(lpvAudioPtr);

        _ASSERT(dwAudioBytes % sizeof(int16_t) == 0);
        const size_t samples = dwAudioBytes / sizeof(int16_t);

        const double logVolume = generator->GetLogarithmicVolume();

        // same formula as QAudio::convertVolume()
        const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);
        const size_t rvolume = size_t(linVolume * 128);

        // it is very uncommon 2 sources play at the same time
        // so we can just add the samples

        for (size_t i = 0; i < samples; ++i)
        {
            *ptr += (data[i] * rvolume) / 128;
            ++ptr;
        }
    }

} // namespace

namespace ra2
{

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName)
    {
        // make sure the size is an integer multiple of the frame size
        const uint32_t bytesPerFrame = nChannels * sizeof(int16_t);
        const uint32_t alignedBufferSize = ((dwBufferSize + bytesPerFrame - 1) / bytesPerFrame) * bytesPerFrame;

        std::shared_ptr<DirectSoundGenerator> generator =
            std::make_shared<DirectSoundGenerator>(alignedBufferSize, nSampleRate, nChannels, pszVoiceName);

        DirectSoundGenerator *ptr = generator.get();
        activeSoundGenerators.insert(ptr);
        return generator;
    }

    void writeAudio(const size_t fps, const size_t sampleRate, const size_t channels, std::vector<int16_t> &buffer)
    {
        // we have already checked that
        // - the buffer is a multiple of the frame size
        // and we always write full frames

        const size_t targetFrames = sampleRate / fps;
        const size_t bytesPerFrame = channels * sizeof(int16_t);
        size_t framesToRead = targetFrames; // for the target sampleRate & channels

        for (const auto &it : activeSoundGenerators)
        {
            const auto &generator = it;
            if (generator->isRunning())
            {
                if (generator->isSameFormat(sampleRate, channels))
                {
                    const size_t bytesAvailable = generator->GetBytesInBuffer();
                    const size_t framesAvailable = bytesAvailable / bytesPerFrame;
                    framesToRead = std::min(framesToRead, framesAvailable);
                }
                else
                {
                    // throw away the audio
                    generator->advanceOneFrame(fps);
                }
            }
        }

        buffer.clear();
        buffer.resize(framesToRead * channels, 0);

        // only generators with the target sampleRate and number of channels are mixed
        const size_t bytesToRead = framesToRead * bytesPerFrame;

        for (const auto &it : activeSoundGenerators)
        {
            const auto &generator = it;
            if (generator->isRunning() && generator->isSameFormat(sampleRate, channels))
            {
                LPVOID lpvAudioPtr1, lpvAudioPtr2;
                DWORD dwAudioBytes1, dwAudioBytes2;
                const size_t bytesRead =
                    generator->Read(bytesToRead, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);
                _ASSERT(bytesRead == bytesToRead);

                int16_t *ptr = buffer.data();

                mixBuffer(generator, lpvAudioPtr1, dwAudioBytes1, ptr);
                mixBuffer(generator, lpvAudioPtr2, dwAudioBytes2, ptr);
            }
        }
        ra2::audio_batch_cb(buffer.data(), framesToRead);
    }

    void bufferStatusCallback(bool active, unsigned occupancy, bool underrun_likely)
    {
        if (active)
        {
            // I am not sure this is any useful
            static unsigned lastOccupancy = 0;
            const int diff = std::abs(int(lastOccupancy) - int(occupancy));
            if (diff >= 5)
            {
                // this is very verbose
                log_cb(
                    RETRO_LOG_INFO, "RA2: %s occupancy = %d, underrun_likely = %d\n", __FUNCTION__, occupancy,
                    underrun_likely);
                lastOccupancy = occupancy;
            }
        }
    }

} // namespace ra2
