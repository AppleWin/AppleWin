#include "StdAfx.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/sdlcompat.h"
#include "frontends/sdl/utils.h"
#include "frontends/common2/programoptions.h"

#include "linux/linuxsoundbuffer.h"

#include "Core.h"
#include "SoundCore.h"
#include "Log.h"

#include <unordered_set>
#include <memory>
#include <iostream>
#include <iomanip>

namespace
{

    // these have to come from EmulatorOptions
    std::string audioDeviceName;
    size_t audioBuffer = 0;

    size_t getBytesPerSecond(const SDL_AudioSpec &spec)
    {
        const size_t bitsPerSample = spec.format & SDL_AUDIO_MASK_BITSIZE;
        const size_t bytesPerFrame = spec.channels * bitsPerSample / 8;
        return spec.freq * bytesPerFrame;
    }

    size_t nextPowerOf2(size_t n)
    {
        size_t k = 1;
        while (k < n)
            k *= 2;
        return k;
    }

    class DirectSoundGenerator : public LinuxSoundBuffer
    {
    public:
        DirectSoundGenerator(
            DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName, const char *deviceName,
            const size_t ms);
        virtual ~DirectSoundGenerator() override;

        virtual HRESULT Stop() override;
        virtual HRESULT Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags) override;

        void printInfo();
        sa2::SoundInfo getInfo();

    private:
        void audioCallback2(uint8_t *stream, int len);
        static void staticAudioCallback2(void *userdata, uint8_t *stream, int len);
        SDL_AudioDeviceID myAudioDevice;

#if SDL_VERSION_ATLEAST(3, 0, 0)
        static void staticAudioCallback3(
            void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);
        SDL_AudioStream *myAudioStream;
#endif

        std::vector<uint8_t> myMixerBuffer;
        SDL_AudioSpec myAudioSpec;

        size_t myBytesPerSecond;

        uint8_t *mixBufferTo(uint8_t *stream);
    };

    std::unordered_set<DirectSoundGenerator *> activeSoundGenerators;

#if SDL_VERSION_ATLEAST(3, 0, 0)
    void DirectSoundGenerator::staticAudioCallback3(
        void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount)
    {
        if (additionalAmount > 0)
        {
            Uint8 *data = SDL_stack_alloc(Uint8, additionalAmount);
            if (data)
            {
                staticAudioCallback2(userdata, data, additionalAmount);
                SDL_PutAudioStreamData(stream, data, additionalAmount);
                SDL_stack_free(data);
            }
        }
    }
#endif

    void DirectSoundGenerator::staticAudioCallback2(void *userdata, uint8_t *stream, int len)
    {
        DirectSoundGenerator *generator = static_cast<DirectSoundGenerator *>(userdata);
        return generator->audioCallback2(stream, len);
    }

    void DirectSoundGenerator::audioCallback2(uint8_t *stream, int len)
    {
        LPVOID lpvAudioPtr1, lpvAudioPtr2;
        DWORD dwAudioBytes1, dwAudioBytes2;
        const size_t bytesRead = Read(len, &lpvAudioPtr1, &dwAudioBytes1, &lpvAudioPtr2, &dwAudioBytes2);

        myMixerBuffer.resize(bytesRead);

        uint8_t *dest = myMixerBuffer.data();
        if (lpvAudioPtr1 && dwAudioBytes1)
        {
            memcpy(dest, lpvAudioPtr1, dwAudioBytes1);
            dest += dwAudioBytes1;
        }
        if (lpvAudioPtr2 && dwAudioBytes2)
        {
            memcpy(dest, lpvAudioPtr2, dwAudioBytes2);
            dest += dwAudioBytes2;
        }

        stream = mixBufferTo(stream);

        const size_t gap = len - bytesRead;
        if (gap)
        {
#if SDL_VERSION_ATLEAST(3, 0, 0)
            const auto silence = SDL_GetSilenceValueForFormat(myAudioSpec.format);
#else
            const auto &silence = myAudioSpec.silence;
#endif
            memset(stream, silence, gap);
        }
    }

    DirectSoundGenerator::DirectSoundGenerator(
        DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName, const char *deviceName,
        const size_t ms)
        : LinuxSoundBuffer(dwBufferSize, nSampleRate, nChannels, pszVoiceName)
        , myAudioDevice(0)
        , myBytesPerSecond(0)
    {
        SDL_zero(myAudioSpec);

#if SDL_VERSION_ATLEAST(3, 0, 0)
        myAudioSpec.freq = mySampleRate;
        myAudioSpec.format = AUDIO_S16LSB;
        myAudioSpec.channels = myChannels;
        myAudioStream =
            SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &myAudioSpec, staticAudioCallback3, this);
        if (myAudioStream)
        {
            myAudioDevice = SDL_GetAudioStreamDevice(myAudioStream);
            myBytesPerSecond = getBytesPerSecond(myAudioSpec);
        }
        else
        {
            throw std::runtime_error(sa2::decorateSDLError("SDL_OpenAudioDeviceStream"));
        }
#else
        SDL_AudioSpec want;
        SDL_zero(want);

        _ASSERT(ms > 0);

        want.freq = mySampleRate;
        want.format = AUDIO_S16LSB;
        want.channels = myChannels;
        want.samples = std::min<size_t>(MAX_SAMPLES, nextPowerOf2(mySampleRate * ms / 1000));
        want.callback = staticAudioCallback2;
        want.userdata = this;
        myAudioDevice = SDL_OpenAudioDevice(deviceName, 0, &want, &myAudioSpec, 0);

        if (myAudioDevice)
        {
            myBytesPerSecond = getBytesPerSecond(myAudioSpec);
        }
        else
        {
            throw std::runtime_error(sa2::decorateSDLError("SDL_OpenAudioDevice"));
        }
#endif
    }

    DirectSoundGenerator::~DirectSoundGenerator()
    {
        activeSoundGenerators.erase(this);
        sa2::compat::pauseAudioDevice(myAudioDevice);
        SDL_CloseAudioDevice(myAudioDevice);
#if SDL_VERSION_ATLEAST(3, 0, 0)
        SDL_DestroyAudioStream(myAudioStream);
#endif
    }

    HRESULT DirectSoundGenerator::Stop()
    {
        const HRESULT res = LinuxSoundBuffer::Stop();
        sa2::compat::pauseAudioDevice(myAudioDevice);
        return res;
    }

    HRESULT DirectSoundGenerator::Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags)
    {
        const HRESULT res = LinuxSoundBuffer::Play(dwReserved1, dwReserved2, dwFlags);
        sa2::compat::resumeAudioDevice(myAudioDevice);
        return res;
    }

    void DirectSoundGenerator::printInfo()
    {
        const uint32_t bytesInBuffer = GetBytesInBuffer();
        std::cerr << "Channels: " << (int)myAudioSpec.channels;
        std::cerr << ", buffer: " << std::setw(6) << bytesInBuffer;
        const double time = double(bytesInBuffer) / myBytesPerSecond * 1000;
        std::cerr << ", " << std::setw(8) << time << " ms";
        std::cerr << ", underruns: " << std::setw(10) << GetBufferUnderruns() << std::endl;
    }

    sa2::SoundInfo DirectSoundGenerator::getInfo()
    {
        DWORD dwStatus;
        GetStatus(&dwStatus);

        sa2::SoundInfo info;
        info.voiceName = myVoiceName;
        info.running = dwStatus & DSBSTATUS_PLAYING;
        info.channels = myChannels;
        info.sampleRate = mySampleRate;
        info.volume = GetLogarithmicVolume();
        info.numberOfUnderruns = GetBufferUnderruns();

        if (info.running && myBytesPerSecond > 0)
        {
            const uint32_t bytesInBuffer = GetBytesInBuffer();
            const float coeff = 1.0 / myBytesPerSecond;
            info.buffer = bytesInBuffer * coeff;
            info.size = myBufferSize * coeff;
        }

        return info;
    }

    uint8_t *DirectSoundGenerator::mixBufferTo(uint8_t *stream)
    {
        // we could copy ADJUST_VOLUME from SDL_mixer.c and avoid all copying and (rare) race conditions
        const double logVolume = GetLogarithmicVolume();
        // same formula as QAudio::convertVolume()
        const double linVolume = logVolume > 0.99 ? 1.0 : -std::log(1.0 - logVolume) / std::log(100.0);

#if SDL_VERSION_ATLEAST(3, 0, 0)
        const float svolume = linVolume;
#else
        const uint8_t svolume = uint8_t(linVolume * SDL_MIX_MAXVOLUME);
#endif

        const size_t len = myMixerBuffer.size();
        memset(stream, 0, len);
        SDL_MixAudioFormat(stream, myMixerBuffer.data(), myAudioSpec.format, len, svolume);
        return stream + len;
    }

} // namespace

namespace sa2
{

    std::shared_ptr<SoundBuffer> iCreateDirectSoundBuffer(
        uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char *pszVoiceName)
    {
        try
        {
            const char *deviceName = audioDeviceName.empty() ? nullptr : audioDeviceName.c_str();

            std::shared_ptr<DirectSoundGenerator> generator = std::make_shared<DirectSoundGenerator>(
                dwBufferSize, nSampleRate, nChannels, pszVoiceName, deviceName, audioBuffer);
            DirectSoundGenerator *ptr = generator.get();
            activeSoundGenerators.insert(ptr);
            return generator;
        }
        catch (const std::exception &e)
        {
            // once this fails, no point in trying again next time
            g_bDisableDirectSound = true;
            g_bDisableDirectSoundMockingboard = true;
            LogOutput("SoundBuffer: %s\n", e.what());
            return nullptr;
        }
    }

    void printAudioInfo()
    {
        for (const auto &it : activeSoundGenerators)
        {
            const auto &generator = it;
            generator->printInfo();
        }
    }

    void resetAudioUnderruns()
    {
        for (const auto &it : activeSoundGenerators)
        {
            const auto &generator = it;
            generator->ResetUnderruns();
        }
    }

    std::vector<SoundInfo> getAudioInfo()
    {
        std::vector<SoundInfo> info;
        info.reserve(activeSoundGenerators.size());

        for (const auto &it : activeSoundGenerators)
        {
            const auto &generator = it;
            info.push_back(generator->getInfo());
        }

        return info;
    }

    void setAudioOptions(const common2::EmulatorOptions &options)
    {
        audioDeviceName = options.audioDeviceName;
        audioBuffer = options.audioBuffer;
    }

} // namespace sa2
